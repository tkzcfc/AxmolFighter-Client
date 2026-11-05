#include "TownScreen.h"

#include "AppContext.h"
#include "DungeonSelectScreen.h"
#include "GameScreen.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "AppContext.h"
#include "core/event/EventDispatcher.h"
#include "core/event/GameEvents.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "resource/builtin/MapResource.h"
#include "ui/battle/LocalRoleUtils.h"
#include "ui/core/FeatureEntry.h"
#include "mugen/render/spine/MgSkeletonAnimation.h"
#include "fairygui/GLoader3D.h"
#include "ui/widgets/common/MessageDialog.h"
#include "ui/town/TownFixed.h"
#include "ui/town/TownSync.h"
#include "ui/widgets/pet/PetLocalState.h"

#include "imgui.h"

#include <net/client_game.pb.h>
#include <net/client_town.pb.h>

#include <algorithm>
#include <chrono>
#include <optional>

using namespace mugen;

namespace gameui
{

namespace
{
constexpr int32_t DEFAULT_TOWN_ID = 41;
// 移动上报频率上限（10 Hz）。
constexpr float REPORT_INTERVAL   = 1.0f / 10.0f;
constexpr float LOGIC_DT          = 1.0f / 30.0f;
constexpr float MAIN_MENU_SLIDE_TIME = 0.25f;

int64_t monotonicNowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

class TownScreen::WorldPresenterHolder
{
public:
    explicit WorldPresenterHolder(ax::Node& root) : root(&root), presenter(std::in_place, root) {}
    ~WorldPresenterHolder()
    {
        // AvatarPresenter::clear 会访问场景节点，必须在摘掉 root 之前销毁 presenter。
        presenter.reset();
        if (root)
            root->removeFromParent();
    }

    ax::Node* root;
    std::optional<mugen::WorldPresenter> presenter;
};

void TownScreen::onPrepareLoad()
{
    const auto* town = conf::Config::instance().find<conf::TownConfig>(m_boot.townId);
    if (!town || town->mapKey.empty())
        return;
    std::string layer = town->mapKey;
    if (!layer.ends_with(".layer"))
        layer = "/mugen/map/" + layer + ".layer";
    else if (!layer.starts_with("/"))
        layer.insert(layer.begin(), '/');
    getResourceLoader().add<gameres::MapResource>(layer);
}

TownScreen::TownScreen()
{
    m_boot.townId = DEFAULT_TOWN_ID;
}

TownScreen::~TownScreen() = default;

TownScreen::TownScreen(TownBootParams boot) : m_boot(std::move(boot))
{
    if (m_boot.townId <= 0)
        m_boot.townId = DEFAULT_TOWN_ID;
}

void TownScreen::onEnter()
{
    Super::onEnter();
    bindMainMenuUi();
    bindJoystick();
    if (!initTown())
    {
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }
    subscribeAppearanceEvents();
    addMessagePushReceiver();
    sendEnterScene();
}

void TownScreen::onExit()
{
    if (m_mainMenu)
    {
        GTween::kill(m_mainMenu, TweenPropType::None, false);
        destroyMainMenu();
    }
    if (m_maskLayer)
    {
        m_maskLayer->setVisible(false);
        m_maskLayer->setTouchable(false);
    }
    m_mainMenuAnimating = false;
    m_joystick.release();
    m_keyboardHeld.fill(false);

    if (m_keyboardListener)
    {
        ax::Director::getInstance()->getEventDispatcher()->removeEventListener(m_keyboardListener);
        m_keyboardListener = nullptr;
    }
    if (auto* ed = AppContext::get().eventDispatcher())
        ed->unsubscribeAll(this);
    if (m_townSync)
        m_townSync->clear();
    m_townSync.reset();
    m_presenter.reset();
    m_initialized = false;
    Super::onExit();
}

void TownScreen::onUpdate(float delta)
{
    if (!m_initialized)
        return;
    applyLocalCorrection();
    m_accumulator    = std::min(m_accumulator + delta, 4.0f * LOGIC_DT);
    uint32_t catchUp = 0;
    while (m_accumulator >= LOGIC_DT && catchUp < 4)
    {
        m_townSync->tick(LOGIC_DT);
        sim::FrameInput input;
        input.frame = m_world.ecs().singleton<sim::WorldClock>().frame + 1;
        std::vector<sim::PlayerCommand> commands;
        // 进入场景前不把本地控制输入喂进模拟：避免在服务器尚未就绪时移动，
        // 使首帧上报被服务器按出生点钳制。
        if (m_enteredScene)
            m_control.advance(commands);
        if (!commands.empty())
            input.players.push_back({0, std::move(commands)});
        input.puppetUpdates = m_townSync->takePuppetUpdates();
        m_world.tick(input);
        m_accumulator -= LOGIC_DT;
        ++catchUp;
    }
    m_presenter->presenter->present(m_world, m_accumulator / LOGIC_DT, delta);
    if (m_enteredScene && m_initialized)
        updatePortalTriggers();
    reportLocalState(delta);
}

void TownScreen::bindMainMenuUi()
{
    m_maskLayer      = getChild<GComponent>("maskLayer");
    m_mainMenuButton = getChild<GComponent>("mainMenuButton");

    m_maskLayer->setVisible(false);
    m_maskLayer->setTouchable(false);
    m_mainMenuButton->setTouchable(true);

    setupMainMenuButtonIcon();

    addClickListener(m_mainMenuButton, AX_CALLBACK_1(TownScreen::onClickMainMenuButton, this));
    addClickListener(m_maskLayer, AX_CALLBACK_1(TownScreen::onClickMaskLayer, this));
}

void TownScreen::setupMainMenuButtonIcon()
{
    auto* loader = m_mainMenuButton->getChild("loaderIcon")->as<GLoader3D>();
    AXASSERT(loader, "TownScreen: mainMenuButton.loaderIcon missing");

    // 主城菜单角色 spine（设计动画名：animation / animation2）。
    constexpr const char* kSkel = "/mugen/spine/ui/afy_maincity/ui_tx_zhucheng_renwucaidan.skel";
    auto* skeleton = mugen::MgSkeletonAnimation::createFromCache(kSkel);
    if (!skeleton)
    {
        AXLOGW("TownScreen: failed to load main menu spine '{}'", kSkel);
        return;
    }

    // Fit the ~100x100 loader; spine atlas is large (1024x2048).
    constexpr float kScale = 0.22f;
    skeleton->setScale(kScale);
    skeleton->setPosition(loader->getWidth() * 0.5f, 8.0f);
    skeleton->setAnimation(0, "animation", true);
    skeleton->setAutoUpdate(true);
    loader->setContent(skeleton);
    AXLOGI("TownScreen: main menu button spine loaded");
}

void TownScreen::onClickMainMenuButton(EventContext*)
{
    if (m_mainMenuAnimating || m_mainMenu)
        return;
    openMainMenu();
}

void TownScreen::onClickMaskLayer(EventContext*)
{
    if (m_mainMenuAnimating || !m_mainMenu)
        return;
    closeMainMenu();
}

void TownScreen::setMainMenuInteractive(bool enabled)
{
    if (m_mainMenu)
        m_mainMenu->setTouchable(enabled);
    if (m_maskLayer)
        m_maskLayer->setTouchable(enabled);
}

void TownScreen::layoutMainMenu(bool offscreen)
{
    const float screenW = getContent()->getWidth();
    const float screenH = getContent()->getHeight();
    const float designH = m_mainMenu->getHeight();
    const float scale   = screenH / designH;
    m_mainMenu->setScale(scale, scale);

    const float scaledW = m_mainMenu->getWidth() * scale;
    const float endX    = screenW - scaledW;
    const float startX  = screenW;
    m_mainMenu->setPosition(offscreen ? startX : endX, 0.0f);
}

void TownScreen::bindMainMenuFeatureButtons()
{
    auto bind = [this](const char* childName, FeatureId featureId) {
        auto* button = m_mainMenu->getChild(childName)->as<GComponent>();
        AXASSERT(button, ("MainMenu child not found: " + std::string(childName)).c_str());
        addClickListener(button, [this, featureId](EventContext*) {
            if (m_mainMenuAnimating)
                return;
            FeatureEntry::open(featureId);
            closeMainMenu();
        });
    };

    bind("dressButton", FeatureId::Dress);
    bind("weaponButton", FeatureId::Weapon);
    bind("equipButton", FeatureId::Equip);
    bind("partnerButton", FeatureId::Partner);
    bind("skillButton", FeatureId::Skill);
    bind("petButton", FeatureId::Pet);
}

void TownScreen::refreshMainMenuPlayerInfo()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !m_mainMenu)
        return;

    auto* nameText  = m_mainMenu->getChild("nameText")->as<GTextField>();
    auto* levelText = m_mainMenu->getChild("levelText")->as<GTextField>();
    nameText->setText(session->selectedCharacter.name);
    levelText->setText(fmt::format("{}级", session->selectedCharacter.level));
}

void TownScreen::openMainMenu()
{
    if (m_mainMenu || m_mainMenuAnimating)
        return;

    m_maskLayer->setVisible(true);
    m_mainMenu = UIPackage::createObject("Screen_Town", "MainMenu")->as<GComponent>();
    getContent()->addChild(m_mainMenu);

    bindMainMenuFeatureButtons();
    refreshMainMenuPlayerInfo();
    layoutMainMenu(true);

    const float screenW = getContent()->getWidth();
    const float scaledW = m_mainMenu->getWidth() * m_mainMenu->getScaleX();
    const float startX  = screenW;
    const float endX    = screenW - scaledW;

    m_mainMenuAnimating = true;
    setMainMenuInteractive(false);

    GTween::to(startX, endX, MAIN_MENU_SLIDE_TIME)
        ->setEase(EaseType::CubicOut)
        ->setTarget(m_mainMenu, TweenPropType::X)
        ->onComplete([this]() {
            m_mainMenuAnimating = false;
            setMainMenuInteractive(true);
        });
}

void TownScreen::closeMainMenu()
{
    if (!m_mainMenu || m_mainMenuAnimating)
        return;

    const float screenW = getContent()->getWidth();
    const float startX  = m_mainMenu->getX();
    const float endX    = screenW;

    m_mainMenuAnimating = true;
    setMainMenuInteractive(false);
    GTween::kill(m_mainMenu, TweenPropType::X, false);

    GTween::to(startX, endX, MAIN_MENU_SLIDE_TIME)
        ->setEase(EaseType::CubicIn)
        ->setTarget(m_mainMenu, TweenPropType::X)
        ->onComplete([this]() {
            destroyMainMenu();
            m_maskLayer->setVisible(false);
            m_maskLayer->setTouchable(false);
            m_mainMenuAnimating = false;
        });
}

void TownScreen::destroyMainMenu()
{
    if (!m_mainMenu)
        return;
    GTween::kill(m_mainMenu, TweenPropType::None, false);
    m_mainMenu->removeFromParent();
    m_mainMenu = nullptr;
}

bool TownScreen::initTown()
{
    if (!conf::Config::instance().find<conf::TownConfig>(m_boot.townId))
    {
        AXLOGE("TownScreen: town {} missing", m_boot.townId);
        return false;
    }

    const auto* town = conf::Config::instance().find<conf::TownConfig>(m_boot.townId);
    m_world.init({1, sim::WorldMode::Town, m_boot.townId, town->mapKey});
    const int32_t spawnX = m_boot.spawnX >= 0 ? m_boot.spawnX : town->actorPosX;
    const int32_t spawnZ = m_boot.spawnZ >= 0 ? m_boot.spawnZ : town->actorPosZ;
    const int32_t localRoleId = resolveLocalPlayerRoleId();
    m_roleRadius              = Fixed::fromInt(conf::Config::instance().getById<conf::RoleConfig>(localRoleId).radius);
    m_localEntity =
        sim::RoleSpawner(m_world).spawnRole({.roleId     = localRoleId,
                                             .level      = 1,
                                             .position   = {Fixed::fromInt(spawnX), {}, Fixed::fromInt(spawnZ)},
                                             .toward     = m_boot.facing < 0 ? -1 : 1,
                                             .playerSlot = 0,
                                             .isAi       = false});
    auto* scene = ax::Director::getInstance()->getRunningScene();
    auto* root  = ax::Node::create();
    scene->addChild(root, 0);
    m_presenter = std::make_unique<WorldPresenterHolder>(*root);
    m_presenter->presenter->loadMap(town->mapKey);
    m_presenter->presenter->setFollowEntity(m_localEntity);
    m_townSync = std::make_unique<TownSync>(m_world, *m_presenter->presenter, *this);
    if (auto* session = AppContext::get().gameSession())
        m_townSync->setLocalPlayerId(session->account.playerID);
    m_presenter->presenter->setEntityPet(m_localEntity, PetLocalState::instance().activeId());
    initPortals();
    if (!initInput())
        return false;
    m_initialized = true;
    return true;
}

void TownScreen::subscribeAppearanceEvents()
{
    auto* ed = AppContext::get().eventDispatcher();
    if (!ed)
        return;
    auto refresh = [this]() { refreshLocalAppearance(); };
    ed->subscribe(game::GameEvent::FashionWornChanged, this, refresh);
    ed->subscribe(game::GameEvent::ArmamentAppearanceChanged, this, refresh);
    ed->subscribe(game::GameEvent::PetFollowChanged, this, [this]() { refreshLocalPet(); });
}

void TownScreen::refreshLocalAppearance()
{
    if (!m_presenter || !m_initialized)
        return;
    if (!m_presenter->presenter->refreshLocalPlayerAppearance(m_world))
        AXLOGW("TownScreen: refreshLocalPlayerAppearance skipped (avatar not ready)");
}

void TownScreen::refreshLocalPet()
{
    if (!m_presenter || !m_initialized)
        return;
    m_presenter->presenter->setEntityPet(m_localEntity, PetLocalState::instance().activeId());
}


void TownScreen::sendEnterScene()
{
    PB::Game::EnterSceneReq req;
    req.set_type(PB::Types::SceneEnterType::Map);
    req.set_map_id(static_cast<uint32_t>(m_boot.townId));
    fillLocalMovement(req.mutable_movement());

    this->call(req, [this](const PB::Game::EnterSceneResp* resp, std::string_view error) {
        if (!resp)
        {
            MessageDialog::showNetErr(error, [this]() { sendEnterScene(); });
            return;
        }

        if (resp->code() != 0)
        {
            MessageDialog::show(resp->message().empty() ? "进入城镇失败" : resp->message());
            return;
        }

        m_enteredScene = true;
        m_townSync->applyEnterPlayers(*resp);
        initializePortalLatches();
        // 新场景进入：服务端已重置校正 pending，本端 ack 状态也重置。
        m_ackCorrectionSeq = 0;
        m_forceReport      = false;
        AXLOGI("TownScreen: entered scene, players={}", resp->players_size());
    });
}

void TownScreen::fillLocalMovement(PB::Types::TownMovement* movement) const
{
    if (!m_initialized)
        return;
    const auto& transform = m_world.ecs().get<sim::Transform>(m_localEntity);
    const auto& motion    = m_world.ecs().get<sim::MotionState>(m_localEntity);
    const auto& status    = m_world.ecs().get<sim::RoleStatus>(m_localEntity);
    auto* pos             = movement->mutable_pos();
    pos->set_x(townToFixedPoint(transform.position.x.toFloatForRender()));
    pos->set_y(townToFixedPoint(transform.position.y.toFloatForRender()));
    pos->set_z(townToFixedPoint(transform.position.z.toFloatForRender()));
    movement->set_facing(transform.towardX < 0 ? 0 : 1);
    movement->set_state(motion.motionName);
    movement->set_moving(status.isMove());
    // 附带当前水平速度（像素/秒），供远端在快照不足时斜向外推。
    const auto& hit = m_world.ecs().get<sim::RoleHitState>(m_localEntity);
    auto* vel       = movement->mutable_vel();
    vel->set_x(hit.forceVelocity.x.toFloatForRender());
    vel->set_y(hit.forceVelocity.y.toFloatForRender());
    vel->set_z(hit.forceVelocity.z.toFloatForRender());
}

void TownScreen::reportLocalState(float delta)
{
    if (!m_enteredScene || !m_initialized)
        return;

    const auto& transform = m_world.ecs().get<sim::Transform>(m_localEntity);
    const auto& motion    = m_world.ecs().get<sim::MotionState>(m_localEntity);
    const auto& status    = m_world.ecs().get<sim::RoleStatus>(m_localEntity);
    const int32_t facing  = transform.towardX < 0 ? 0 : 1;
    const bool moving     = status.isMove();

    const bool changed = facing != m_lastFacing || moving != m_lastMoving || motion.motionName != m_lastState;
    m_reportTimer += delta;
    const bool periodic = moving && m_reportTimer >= REPORT_INTERVAL;
    const bool force    = m_forceReport;
    if (!changed && !periodic && !force)
        return;

    PB::Town::TownMoveReport report;
    report.set_client_seq(++m_clientSeq);
    report.set_client_time_ms(static_cast<uint64_t>(monotonicNowMs()));
    report.set_ack_correction_seq(m_ackCorrectionSeq);
    fillLocalMovement(report.mutable_movement());
    this->notify(report);

    m_reportTimer  = 0.0f;
    m_forceReport  = false;
    m_lastFacing   = facing;
    m_lastMoving   = moving;
    m_lastState    = motion.motionName;
}

void TownScreen::applyLocalCorrection()
{
    if (!m_pendingCorrection)
        return;
    m_world.ecs().get<sim::Transform>(m_localEntity).position = *m_pendingCorrection;
    m_pendingCorrection.reset();
    // 校正已吸附到权威位置：确认该校正序号，并强制下一帧立即上报 ack。
    m_ackCorrectionSeq = m_lastCorrectionSeq;
    m_forceReport      = true;
}

void TownScreen::addMessagePushReceiver()
{
    if (m_townSync)
        m_townSync->start();

    this->listenPush([this](PB::Town::TownPositionCorrectionPush& push) {
        // 进入场景前收到的校正可能来自上一个城镇，直接忽略，避免把玩家拉回旧场景。
        if (!m_enteredScene)
            return;
        if (push.client_seq() < m_lastCorrectionSeq)
            return;  // 过期校正，忽略
        m_lastCorrectionSeq = push.client_seq();
        if (!push.has_pos())
            return;
        m_pendingCorrection = mugen::FixedVec3{townFromFixedPoint(push.pos().x()),
                                                townFromFixedPoint(push.pos().y()),
                                                townFromFixedPoint(push.pos().z())};
        AXLOGD("TownScreen: position correction seq={} -> ({},{},{})", push.client_seq(), push.pos().x(),
               push.pos().y(), push.pos().z());
    });

    this->listenPush([this](PB::Game::DuelInvitePush& push) {
        m_duelInviterId   = push.from_player_id();
        m_duelInviterName = push.from_name();
        m_showDuelInvite  = true;
    });

    this->listenPush([this](PB::Game::DuelStartPush& push) {
        NetBattleParams params;
        params.battleId        = push.battle_id();
        params.roomId          = push.room_id();
        params.opponentPlayerId = push.opponent_player_id();
        params.opponentName    = push.opponent_name();
        AXLOGI("TownScreen: duel started, entering net battle {}", params.battleId);
        getScreenManager()->switchScreen<GameScreen>(params);
    });

    this->listenPush([this](PB::Game::DuelEndPush& push) {
        m_showDuelInvite = false;
        MessageDialog::show(push.message().empty() ? "决斗已取消" : push.message());
    });
}

void TownScreen::initPortals()
{
    m_portals.clear();
    const auto* town = conf::Config::instance().find<conf::TownConfig>(m_boot.townId);
    if (!town || town->portalIds.empty())
    {
        AXLOGI("TownScreen: town {} has no portals", m_boot.townId);
        return;
    }

    const auto valueAt = [](const std::vector<int32_t>& values, size_t index) {
        return index < values.size() ? values[index] : 0;
    };
    const auto firstAt = [](const std::vector<std::vector<int32_t>>& values, size_t index) {
        return index < values.size() && !values[index].empty() ? values[index].front() : 0;
    };
    for (size_t index = 0; index < town->portalIds.size(); ++index)
    {
        const auto id   = town->portalIds[index];
        const auto slot = valueAt(town->portalSlots, index);
        if (id <= 0 || slot <= 0)
            continue;

        TownPortal portal;
        portal.portalId = id;
        portal.slot     = slot;
        portal.posX     = Fixed::fromInt(valueAt(town->portalPosX, index));
        portal.posZ     = Fixed::fromInt(valueAt(town->portalPosZ, index));
        if (const auto* portalRow = conf::Config::instance().find<conf::PortalConfig>(id))
            portal.radius = Fixed::fromInt(portalRow->radius);
        portal.destType = valueAt(town->portalDestType, index);
        if (index < town->portalDestRoomIds.size() && !town->portalDestRoomIds[index].empty())
        {
            const auto realRoomId = firstAt(town->portalDestRealRoomIds, index);
            portal.destTownId     = realRoomId > 0 ? realRoomId : firstAt(town->portalDestRoomIds, index);
            portal.destPosX       = firstAt(town->portalDestPosX, index);
            portal.destPosZ       = firstAt(town->portalDestPosZ, index);
            const auto facing     = firstAt(town->portalDestVectorX, index);
            portal.destFacing     = facing != 0 ? facing : 1;
        }
        auto* node = ax::Node::create();
        node->setPosition(static_cast<float>(valueAt(town->portalPosX, index)),
                          static_cast<float>(valueAt(town->portalPosZ, index)));
        m_presenter->presenter->addMapNode(*node);
        portal.node = node;
        m_portals.push_back(portal);
    }
}

bool TownScreen::initInput()
{
    auto* scene = ax::Director::getInstance()->getRunningScene();
    if (!scene)
    {
        AXLOGE("TownScreen: running scene is null");
        return false;
    }
    using Key                         = ax::EventKeyboard::KeyCode;
    using ControlKey                  = sim::ControlKey;
    m_slotMap[Key::KEY_LEFT_ARROW]    = static_cast<uint32_t>(ControlKey::Left);
    m_slotMap[Key::KEY_RIGHT_ARROW]   = static_cast<uint32_t>(ControlKey::Right);
    m_slotMap[Key::KEY_UP_ARROW]      = static_cast<uint32_t>(ControlKey::Up);
    m_slotMap[Key::KEY_DOWN_ARROW]    = static_cast<uint32_t>(ControlKey::Down);
    m_keyboardListener                = ax::EventListenerKeyboard::create();
    m_keyboardListener->onKeyPressed  = AX_CALLBACK_2(TownScreen::onKeyPressed, this);
    m_keyboardListener->onKeyReleased = AX_CALLBACK_2(TownScreen::onKeyReleased, this);
    ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(m_keyboardListener,
                                                                                              scene);
    return true;
}

void TownScreen::onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event*)
{
    setInputFromKey(code, true);
}

void TownScreen::onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event*)
{
    setInputFromKey(code, false);
}

void TownScreen::setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed)
{
    const auto it = m_slotMap.find(code);
    if (it == m_slotMap.end())
        return;
    const auto key = static_cast<sim::ControlKey>(it->second);
    const auto idx = static_cast<size_t>(key);
    if (idx < m_keyboardHeld.size())
        m_keyboardHeld[idx] = pressed;
    refreshControlKey(key);
}

void TownScreen::bindJoystick()
{
    auto* joystick = dynamic_cast<GComponent*>(getContent()->getChild("joystick"));
    if (!m_joystick.bind(joystick, [this](sim::ControlKey key, bool /*pressed*/) { refreshControlKey(key); }))
    {
        static bool s_warnedMissingJoystick = false;
        if (!s_warnedMissingJoystick)
        {
            s_warnedMissingJoystick = true;
            AXLOGW("TownScreen: joystick is missing in published Screen_Town.fui; skipping virtual joystick");
        }
    }
}

void TownScreen::refreshControlKey(sim::ControlKey key)
{
    const auto idx          = static_cast<size_t>(key);
    const bool keyboardHeld = idx < m_keyboardHeld.size() && m_keyboardHeld[idx];
    m_control.setKey(key, keyboardHeld || m_joystick.isDirectionPressed(key));
}

bool TownScreen::portalContains(const TownPortal& portal) const
{
    const auto& t   = m_world.ecs().get<sim::Transform>(m_localEntity);
    const Fixed dx  = t.position.x - portal.posX;
    const Fixed dz  = t.position.z - portal.posZ;
    const Fixed sum = portal.radius + m_roleRadius;
    return dx * dx + dz * dz <= sum * sum;
}

void TownScreen::initializePortalLatches()
{
    for (auto& portal : m_portals)
        portal.inside = portalContains(portal);
}

void TownScreen::updatePortalTriggers()
{
    if (m_portalTriggered)
        return;
    for (auto& portal : m_portals)
    {
        const bool inside = portalContains(portal);
        if (inside && !portal.inside)
        {
            portal.inside      = true;
            m_portalTriggered  = true;
            onPortalTriggered(portal);
            return;
        }
        portal.inside = inside;
    }
}

void TownScreen::onPortalTriggered(const TownPortal& portal)
{
    constexpr int32_t kDestTypeFunction = 1;
    constexpr int32_t kDestTypeCity     = 5;

    AXLOGI("TownScreen: portal triggered id={} slot={} destType={} destTown={}", portal.portalId, portal.slot,
           portal.destType, portal.destTownId);

    if (portal.destType == kDestTypeFunction)
    {
        getScreenManager()->pushScreen<DungeonSelectScreen>();
        return;
    }

    if (portal.destType == kDestTypeCity)
    {
        if (portal.destTownId <= 0 || !conf::Config::instance().find<conf::TownConfig>(portal.destTownId))
        {
            MessageDialog::show("目标城镇配置不存在");
            return;
        }

        TownBootParams boot;
        boot.townId = portal.destTownId;
        boot.spawnX = portal.destPosX;
        boot.spawnZ = portal.destPosZ;
        boot.facing = portal.destFacing;
        getScreenManager()->switchScreen<TownScreen>(boot);
        return;
    }

    AXLOGI("TownScreen: unsupported portal destType={}", portal.destType);
}

void TownScreen::sendDuelInvite(int64_t targetPlayerId)
{
    if (targetPlayerId == 0)
        return;

    auto* session = AppContext::get().gameSession();
    if (session && targetPlayerId == session->account.playerID)
    {
        MessageDialog::show("不能和自己决斗");
        return;
    }

    PB::Game::DuelInviteReq req;
    req.set_target_player_id(targetPlayerId);
    this->call(req, [this](const PB::Game::DuelInviteResp* resp, std::string_view err) {
        if (!resp)
        {
            MessageDialog::showNetErr(err);
            return;
        }
        if (resp->code() != 0)
            MessageDialog::show(resp->message().empty() ? "决斗邀请失败" : resp->message());
    });
}

void TownScreen::respondDuelInvite(int32_t accept)
{
    PB::Game::DuelRespondReq req;
    req.set_accept(accept);
    this->call(req, [this](const PB::Game::DuelRespondResp* resp, std::string_view err) {
        if (!resp)
        {
            MessageDialog::showNetErr(err);
            return;
        }
        if (resp->code() != 0)
            MessageDialog::show(resp->message().empty() ? "决斗回应失败" : resp->message());
    });
}

void TownScreen::onImGUIRender()
{
    ImGui::SetNextWindowPos(ImVec2(20.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260.0f, 280.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("城镇");

    if (!m_enteredScene)
        ImGui::TextUnformatted("正在进入城镇...");
    else
        ImGui::TextUnformatted("Town is being rebuilt");

    ImGui::Separator();
    ImGui::Text("当前城镇: %d", m_boot.townId);
    ImGui::Text("传送门: %d", static_cast<int>(m_portals.size()));
    for (size_t i = 0; i < m_portals.size(); ++i)
    {
        const auto& portal = m_portals[i];
        ImGui::Text("  slot=%d id=%d dest=%d -> town %d", portal.slot, portal.portalId, portal.destType,
                    portal.destTownId);
        if (portal.destType == 5 && portal.destTownId > 0)
        {
            const std::string btn = fmt::format("传送到城镇 {}##portal_{}", portal.destTownId, i);
            if (ImGui::Button(btn.c_str(), ImVec2(-1, 0)))
                onPortalTriggered(portal);
        }
        else if (portal.destType == 1)
        {
            const std::string btn = fmt::format("打开副本选择##portal_{}", i);
            if (ImGui::Button(btn.c_str(), ImVec2(-1, 0)))
                onPortalTriggered(portal);
        }
    }

    if (ImGui::Button("打开副本选择", ImVec2(-1, 0)))
        getScreenManager()->pushScreen<DungeonSelectScreen>();

    if (ImGui::Button(m_mainMenu ? "关闭主菜单" : "打开主菜单", ImVec2(-1, 0)))
    {
        if (m_mainMenu)
            closeMainMenu();
        else
            openMainMenu();
    }

    ImGui::Separator();
    const auto remotes = m_townSync ? m_townSync->remotePlayers() : std::vector<TownSync::RemotePlayerInfo>{};
    ImGui::Text("同场景玩家: %d", static_cast<int>(remotes.size()));
    if (remotes.empty())
        ImGui::TextDisabled("  (暂无其他玩家)");
    for (const auto& remote : remotes)
    {
        ImGui::Text("  %s  Lv%d  职业%d", remote.name.empty() ? "???" : remote.name.c_str(), remote.level,
                    remote.classId);
        ImGui::SameLine();
        const std::string btn = fmt::format("邀请决斗##duel_{}", remote.playerId);
        ImGui::BeginDisabled(!m_enteredScene);
        if (ImGui::SmallButton(btn.c_str()))
            sendDuelInvite(remote.playerId);
        ImGui::EndDisabled();
    }

    if (m_showDuelInvite)
    {
        ImGui::OpenPopup("duel_invite");
        m_showDuelInvite = false;
    }
    if (ImGui::BeginPopupModal("duel_invite", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s 邀请你决斗", m_duelInviterName.empty() ? "???" : m_duelInviterName.c_str());
        ImGui::Separator();
        if (ImGui::Button("接受", ImVec2(120, 0)))
        {
            respondDuelInvite(1);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("拒绝", ImVec2(120, 0)))
        {
            respondDuelInvite(0);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}  // namespace gameui
