#include "ui/battle/LocalBattleMode.h"

#include "axmol.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "AppContext.h"
#include "core/event/EventDispatcher.h"
#include "core/event/GameEvents.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/input/FrameInput.h"
#include "mugen/sim/present/PresentationEvent.h"
#include "mugen/sim/view/HudView.h"
#include "mugen/sim/view/WorldView.h"
#include "ui/battle/LocalRoleUtils.h"
#include "ui/widgets/pet/PetLocalState.h"
#include "imgui.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>
#include <variant>

namespace gameui
{
namespace
{

// 按房间查询默认关卡与副本，保持离线入口与无头回放使用同一初始化参数。
std::pair<int32_t, int32_t> resolveStageAndCopy(int32_t roomId)
{
    auto& config    = mugen::conf::Config::instance();
    int32_t stageId = 0;
    int32_t copyId  = 0;
    for (const auto& stage : config.table<mugen::conf::StageConfig>())
    {
        if (stage.roomId == roomId)
        {
            stageId = stage.id;
            break;
        }
    }
    for (const auto& copy : config.table<mugen::conf::CopyConfig>())
    {
        if (copy.stageId == stageId)
        {
            copyId = copy.id;
            break;
        }
    }
    return {stageId, copyId};
}
}  // namespace

class LocalBattleMode::WorldPresenterHolder
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

LocalBattleMode::LocalBattleMode() = default;

LocalBattleMode::LocalBattleMode(int32_t roomId) : m_roomId(roomId) {}

LocalBattleMode::LocalBattleMode(LocalBattleParams params)
    : m_roomId(params.roomId), m_roleId(params.roleId), m_petId(params.petId), m_autotestFrames(params.autotestFrames)
{}

LocalBattleMode::~LocalBattleMode() = default;

bool LocalBattleMode::init()
{
    auto& config     = mugen::conf::Config::instance();
    const auto* room = config.find<mugen::conf::RoomConfig>(m_roomId);
    if (!room)
    {
        AXLOGE("Room not found: {}", m_roomId);
        std::cerr << "Room not found\n";
        std::exit(1);
        return false;
    }
    const int32_t roleId         = m_roleId > 0 ? m_roleId : resolveLocalPlayerRoleId();
    const auto [stageId, copyId] = resolveStageAndCopy(m_roomId);
    mugen::sim::WorldSetup setup;
    setup.seed      = 1;
    setup.mode      = mugen::sim::WorldMode::Battle;
    setup.roomId    = m_roomId;
    setup.mapKey    = room->mapKey;
    setup.aiEnabled = true;
    mugen::sim::PlayerSetup playerSetup{.slot = 0, .roleId = roleId, .level = 1};
    setup.players.push_back(playerSetup);
    setup.stageId          = stageId;
    setup.copyId           = copyId;
    const auto players     = mugen::sim::bootstrapBattle(m_world, setup);
    if (players.empty())
        return false;
    const mugen::Entity player = players.front();
    auto* scene                = ax::Director::getInstance()->getRunningScene();
    auto* root                 = ax::Node::create();
    scene->addChild(root, 10);
    m_presenter = std::make_unique<WorldPresenterHolder>(*root);
    m_presenter->presenter->loadMap(room->mapKey);
    m_presenter->presenter->setFollowEntity(player);
    m_presenter->presenter->setPet(resolveFollowPetId());
    m_localPlayer = player;
    m_replay      = {};
    m_replay.configSchemaHash = mugen::conf::configSchemaHash();
    m_replay.setup = setup;
    m_initialized  = true;
    subscribeAppearanceEvents();
    AXLOGI("LocalBattleMode: ready (room={})", m_roomId);
    return true;
}

void LocalBattleMode::onUpdate(float delta)
{
    if (!m_initialized)
        return;
    m_accumulator    = std::min(m_accumulator + delta, 4.0f / 30.0f);
    uint32_t catchUp = 0;
    while (m_accumulator >= (1.0f / 30.0f) && catchUp < 4 && (!m_debugPaused || m_debugSingleStep))
    {
        mugen::sim::FrameInput input;
        input.frame          = mugen::sim::view::worldFrame(m_world) + 1;
        input.systemCommands = std::move(m_systemCommands);
        m_systemCommands.clear();
        std::vector<mugen::sim::PlayerCommand> commands;
        m_control.advance(commands);
        if (!commands.empty())
            input.players.push_back({0, std::move(commands)});
        m_world.tick(input);
        for (const auto& event : mugen::sim::view::worldEvents(m_world))
        {
            using namespace mugen::sim;
            if (std::holds_alternative<HurtNumber>(event.payload))
            {
                const auto info = mugen::sim::view::readEntityRoleInfo(m_world, event.entity);
                if (info.valid && info.roleType != static_cast<int32_t>(mugen::EntityRoleType::kHero))
                    m_lastHurtTarget = event.entity;
            }
            else if (std::holds_alternative<Victory>(event.payload))
            {
                m_roomMessage    = "Victory";
                const auto room = mugen::sim::view::readBattleRoom(m_world, m_localPlayer);
                if (!room.hasNextRoom)
                    latchResult(true);
            }
            else if (std::holds_alternative<Failure>(event.payload))
            {
                m_roomMessage = "Defeat";
                latchResult(false);
            }
            else if (const auto* warning = std::get_if<RoomWarning>(&event.payload))
            {
                m_roomMessage = warning->boss ? "Boss incoming" : "Warning";
            }
            else if (const auto* tip = std::get_if<Tips>(&event.payload))
                m_roomMessage = "Room tip: " + tip->text;
            else if (const auto* change = std::get_if<RoomStateChanged>(&event.payload))
                if (change->state == static_cast<int32_t>(mugen::RoomState::FightRevive))
                    m_roomMessage = "Revive available";
        }
        if (!m_replayPath.empty())
            m_replay.frames.push_back(input);
        m_accumulator -= 1.0f / 30.0f;
        ++catchUp;
        m_debugSingleStep = false;
    }
    m_presenter->presenter->present(m_world, m_accumulator * 30.0f, delta);
    if (m_autotestFrames > 0 && mugen::sim::view::worldFrame(m_world) >= static_cast<uint64_t>(m_autotestFrames))
    {
        const uint64_t hash = m_world.hash();
        AXLOGI("world hash {} {:016x}", m_autotestFrames, hash);
        std::cout << fmt::format("world hash {} {:016x}\n", m_autotestFrames, hash);
        std::cout.flush();
        std::exit(0);
    }
}

void LocalBattleMode::onExit()
{
    if (!m_replayPath.empty() && !m_replay.frames.empty())
    {
        m_replay.finalWorldHash = m_world.hash();
        std::string error;
        if (!m_replay.save(m_replayPath, error))
            AXLOGE("LocalBattleMode: replay save failed: {}", error);
    }
    if (auto* ed = AppContext::get().eventDispatcher())
        ed->unsubscribeAll(this);
    m_presenter.reset();
    m_initialized = false;
}

void LocalBattleMode::latchResult(bool win)
{
    if (m_result)
        return;
    DungeonBattleResult result;
    result.win = win;
    const auto room       = mugen::sim::view::readBattleRoom(m_world, m_localPlayer);
    result.copyId         = room.copyId;
    result.roomId         = room.roomId;
    const int64_t seconds = room.gameSeconds.toInt();
    result.timeSec        = seconds > 0 ? static_cast<int32_t>(seconds) : 0;
    result.maxCombo       = room.maxCombo;
    result.maxEvaluate    = room.maxEvaluate;
    m_result = result;
    // 同一帧不再继续推进，结算界面打开前战斗停住。
    m_debugPaused = true;
}

void LocalBattleMode::subscribeAppearanceEvents()
{
    auto* ed = AppContext::get().eventDispatcher();
    if (!ed)
        return;
    auto refresh = [this]() { refreshLocalAppearance(); };
    ed->subscribe(game::GameEvent::FashionWornChanged, this, refresh);
    ed->subscribe(game::GameEvent::ArmamentAppearanceChanged, this, refresh);
    ed->subscribe(game::GameEvent::PetFollowChanged, this, [this]() { refreshFollowPet(); });
}

void LocalBattleMode::refreshLocalAppearance()
{
    if (!m_presenter || !m_initialized)
        return;
    if (!m_presenter->presenter->refreshLocalPlayerAppearance(m_world))
        AXLOGW("LocalBattleMode: refreshLocalPlayerAppearance skipped (avatar not ready)");
}

int32_t LocalBattleMode::resolveFollowPetId() const
{
    if (m_petId > 0)
        return m_petId;
    const int32_t activeId = PetLocalState::instance().activeId();
    return activeId > 0 ? activeId : 17102;
}

void LocalBattleMode::refreshFollowPet()
{
    if (!m_presenter || !m_initialized)
        return;
    m_presenter->presenter->setPet(resolveFollowPetId());
}

void LocalBattleMode::renderDebugPanel()
{
    if (m_initialized)
    {
        auto& debugDraw     = m_presenter->presenter->debugDraw();
        bool collisionDebug = debugDraw.enabled();
        m_debugPanel.render(m_world, m_debugPaused, m_debugSingleStep, collisionDebug);
        debugDraw.setEnabled(collisionDebug);

        const auto room = mugen::sim::view::readBattleRoom(m_world, m_localPlayer);
        ImGui::Begin("Room");
        ImGui::TextUnformatted(m_roomMessage.c_str());
        ImGui::Text("State %d", static_cast<int>(room.state));
        ImGui::Text("Wave %d / %d", room.wave, room.totalWaves);
        ImGui::Text("Revives %d / %d", room.usedRevives, room.maxRevives);
        if (room.state == mugen::RoomState::ReadySpine && room.pendingEmpty)
            if (ImGui::Button("Continue"))
                m_systemCommands.push_back(mugen::sim::RoomWarningDone{m_roomId});
        ImGui::BeginDisabled(!room.reviving || room.failure);
        if (ImGui::Button("Revive"))
            m_systemCommands.push_back(mugen::sim::ReviveCommand{});
        ImGui::EndDisabled();
        if (ImGui::Button("Back to town"))
            requestReturnToTown();
        ImGui::End();
    }
}

void LocalBattleMode::toggleCollisionDebug()
{
    if (!m_initialized)
        return;
    auto& debugDraw = m_presenter->presenter->debugDraw();
    debugDraw.setEnabled(!debugDraw.enabled());
}

void LocalBattleMode::setInput(uint32_t slot, bool pressed)
{
    if (slot < static_cast<uint32_t>(mugen::sim::ControlKey::Count))
        m_control.setKey(static_cast<mugen::sim::ControlKey>(slot), pressed);
}

}  // namespace gameui
