#include "TownScreen.h"

#include "AppContext.h"
#include "DungeonSelectScreen.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "resource/builtin/MapResource.h"
#include "ui/battle/LocalRoleUtils.h"
#include "ui/widgets/common/MessageDialog.h"

#include "imgui.h"

#include <net/client_game.pb.h>
#include <net/client_town.pb.h>

#include <algorithm>

using namespace mugen;

namespace gameui
{

namespace
{
constexpr int32_t DEFAULT_TOWN_ID = 41;
constexpr float REPORT_INTERVAL   = 0.2f;
constexpr float LOGIC_DT          = 1.0f / 30.0f;

mugen::Fixed fixedFromNetwork(float value)
{
    return mugen::Fixed::fromRaw(static_cast<int64_t>(value * static_cast<float>(mugen::Fixed::kOne)));
}
}  // namespace

class TownScreen::WorldPresenterHolder
{
public:
    explicit WorldPresenterHolder(ax::Node& root) : root(&root), presenter(root) {}
    ~WorldPresenterHolder()
    {
        if (root)
            root->removeFromParent();
    }

    ax::Node* root;
    mugen::WorldPresenter presenter;
};

void TownScreen::onPrepareLoad()
{
    const auto* town = conf::Config::instance().find<conf::TownConfig>(m_boot.townId);
    if (!town || town->mapKey.empty())
        return;
    const std::string layer = town->mapKey.ends_with(".layer") ? town->mapKey : "mugen/map/" + town->mapKey + ".layer";
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
    if (!initTown())
    {
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }
    addMessagePushReceiver();
    sendEnterScene();
}

void TownScreen::onExit()
{
    if (m_keyboardListener)
    {
        ax::Director::getInstance()->getEventDispatcher()->removeEventListener(m_keyboardListener);
        m_keyboardListener = nullptr;
    }
    m_presenter.reset();
    m_initialized = false;
    Super::onExit();
}

void TownScreen::onUpdate(float delta)
{
    if (!m_initialized)
        return;
    m_accumulator    = std::min(m_accumulator + delta, 4.0f * LOGIC_DT);
    uint32_t catchUp = 0;
    while (m_accumulator >= LOGIC_DT && catchUp < 4)
    {
        sim::FrameInput input;
        input.frame = m_world.ecs().singleton<sim::WorldClock>().frame + 1;
        std::vector<sim::PlayerCommand> commands;
        m_control.advance(commands);
        if (!commands.empty())
            input.players.push_back({0, std::move(commands)});
        input.puppetUpdates = std::move(m_pendingPuppetUpdates);
        m_pendingPuppetUpdates.clear();
        m_world.tick(input);
        m_accumulator -= LOGIC_DT;
        ++catchUp;
    }
    m_presenter->presenter.present(m_world, m_accumulator / LOGIC_DT, delta);
    reportLocalState(delta);
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
    m_localEntity =
        sim::RoleSpawner(m_world).spawnRole({.roleId     = resolveLocalPlayerRoleId(),
                                             .level      = 1,
                                             .position   = {Fixed::fromInt(spawnX), {}, Fixed::fromInt(spawnZ)},
                                             .toward     = m_boot.facing < 0 ? -1 : 1,
                                             .playerSlot = 0,
                                             .isAi       = false});
    auto* scene = ax::Director::getInstance()->getRunningScene();
    auto* root  = ax::Node::create();
    scene->addChild(root, 10);
    m_presenter = std::make_unique<WorldPresenterHolder>(*root);
    m_presenter->presenter.loadMap(town->mapKey);
    m_presenter->presenter.setFollowEntity(m_localEntity);
    initPortals();
    if (!initInput())
        return false;
    m_initialized = true;
    return true;
}

void TownScreen::sendEnterScene()
{
    PB::Game::EnterSceneReq req;
    req.set_type(PB::Types::SceneEnterType::Map);
    req.set_map_id(static_cast<uint32_t>(m_boot.townId));
    fillLocalState(req.mutable_state());

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
        for (const auto& player : resp->players())
            addOrUpdateRemotePlayer(player);
        AXLOGI("TownScreen: entered scene, players={}", resp->players_size());
    });
}

void TownScreen::fillLocalState(PB::Types::PlayerState* state) const
{
    auto* session = AppContext::get().gameSession();
    if (!session)
        return;

    state->set_player_id(session->account.playerID);
    state->set_class_id(session->selectedCharacter.classID);
    state->set_name(session->selectedCharacter.name);
    if (!m_initialized)
        return;
    const auto& transform = m_world.ecs().get<sim::Transform>(m_localEntity);
    const auto& motion    = m_world.ecs().get<sim::MotionState>(m_localEntity);
    const auto& status    = m_world.ecs().get<sim::RoleStatus>(m_localEntity);
    state->set_pos_x(transform.position.x.toFloatForRender());
    state->set_pos_y(transform.position.y.toFloatForRender());
    state->set_pos_z(transform.position.z.toFloatForRender());
    state->set_facing(transform.towardX < 0 ? 0 : 1);
    state->set_state(motion.motionName);
    state->set_moving(status.eEntityStatus == EntityRoleStatus::Walk);
}

void TownScreen::reportLocalState(float delta)
{
    if (!m_enteredScene)
        return;

    m_reportTimer += delta;
    if (m_reportTimer < REPORT_INTERVAL)
        return;
    m_reportTimer = 0.0f;

    PB::Types::PlayerState state;
    fillLocalState(&state);

    PB::Town::TownPlayerStatePush push;
    *push.mutable_state() = state;
    this->notify(push);
}

void TownScreen::addMessagePushReceiver()
{
    this->listenPush([this](PB::Town::ScenePlayerEnterPush& push) {
        if (!push.has_state())
            return;
        addOrUpdateRemotePlayer(push.state());
        AXLOGI("TownScreen: player {} entered scene", push.state().player_id());
    });

    this->listenPush([this](PB::Town::ScenePlayerLeavePush& push) {
        if (m_selectedPlayerId == push.player_id())
            m_selectedPlayerId = 0;
        removeRemotePlayer(push.player_id());
        AXLOGI("TownScreen: player {} left scene", push.player_id());
    });

    this->listenPush([this](PB::Town::ScenePlayerStatePush& push) {
        if (!push.has_state())
            return;
        addOrUpdateRemotePlayer(push.state());
    });

    this->listenPush([this](PB::Game::DuelInvitePush& push) {
        m_duelInviterId   = push.from_player_id();
        m_duelInviterName = push.from_name();
        m_showDuelInvite  = true;
    });

    this->listenPush([this](PB::Game::DuelStartPush&) { AXLOGI("Online battle disabled during rewrite"); });

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
        m_presenter->presenter.addMapNode(*node);
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
    if (it != m_slotMap.end())
        m_control.setKey(static_cast<sim::ControlKey>(it->second), pressed);
}

mugen::Entity TownScreen::localEntity() const
{
    return m_localEntity;
}

void TownScreen::queuePuppetUpdate(mugen::Entity entity, const PB::Types::PlayerState& state)
{
    m_pendingPuppetUpdates.push_back(
        {entity,
         {fixedFromNetwork(state.pos_x()), fixedFromNetwork(state.pos_y()), fixedFromNetwork(state.pos_z())},
         state.facing() == 0 ? -1 : 1,
         state.moving() ? EntityRoleStatus::Walk : EntityRoleStatus::Idle,
         state.state()});
}

void TownScreen::addOrUpdateRemotePlayer(const PB::Types::PlayerState& state)
{
    auto* session = AppContext::get().gameSession();
    if (!session || state.player_id() == session->account.playerID)
        return;
    auto [it, inserted] = m_remotePlayers.try_emplace(state.player_id());
    if (inserted)
    {
        const int32_t roleId = type_conversions::toRoleConfigId(static_cast<CharacterClass>(state.class_id()));
        it->second           = sim::RoleSpawner(m_world).spawnPuppet(state.player_id(), roleId, {}, 1);
    }
    queuePuppetUpdate(it->second, state);
}

void TownScreen::removeRemotePlayer(int64_t playerId)
{
    const auto it = m_remotePlayers.find(playerId);
    if (it == m_remotePlayers.end())
        return;
    m_world.destroy(it->second);
    m_remotePlayers.erase(it);
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
