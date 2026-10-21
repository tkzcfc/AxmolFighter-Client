#include "ui/battle/LocalBattleMode.h"

#include "axmol.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "ui/battle/LocalRoleUtils.h"
#include "mugen/sim/room/RoomDirector.h"
#include "imgui.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace gameui
{
class LocalBattleMode::WorldPresenterHolder
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

LocalBattleMode::LocalBattleMode() = default;

LocalBattleMode::LocalBattleMode(int32_t roomId) : m_roomId(roomId) {}

LocalBattleMode::LocalBattleMode(LocalBattleParams params)
    : m_roomId(params.roomId), m_roleId(params.roleId), m_autotestFrames(params.autotestFrames)
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
    m_world.init({1, mugen::sim::WorldMode::Battle, m_roomId, room->mapKey});

    const int32_t roleId       = m_roleId > 0 ? m_roleId : resolveLocalPlayerRoleId();
    const auto characterClass  = mugen::type_conversions::toCharacterClass(roleId);
    const mugen::Entity player = mugen::sim::RoleSpawner(m_world).spawnPlayer(
        characterClass, {.roleId = roleId, .level = 1, .playerSlot = 0, .isAi = false});
    int32_t stageId = 0, copyId = 0;
    for (const auto& stage : config.table<mugen::conf::StageConfig>())
    {
        if (stage.roomId == m_roomId)
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
    if (!mugen::sim::room::RoomDirector(m_world).enter(*room, stageId, copyId))
        return false;
    auto* scene = ax::Director::getInstance()->getRunningScene();
    auto* root  = ax::Node::create();
    scene->addChild(root, 10);
    m_presenter = std::make_unique<WorldPresenterHolder>(*root);
    m_presenter->presenter.loadMap(room->mapKey);
    m_presenter->presenter.setFollowEntity(player);
    m_replay.schemaHash = mugen::conf::configSchemaHash();
    m_replay.setup      = m_world.setup();
    m_initialized       = true;
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
        input.frame          = m_world.ecs().singleton<mugen::sim::WorldClock>().frame + 1;
        input.systemCommands = std::move(m_systemCommands);
        m_systemCommands.clear();
        std::vector<mugen::sim::PlayerCommand> commands;
        m_control.advance(commands);
        if (!commands.empty())
            input.players.push_back({0, std::move(commands)});
        m_world.tick(input);
        for (const auto& event : m_world.ecs().singleton<mugen::sim::PresentationEvents>().events)
        {
            using namespace mugen::sim;
            if (std::holds_alternative<HurtNumber>(event.payload))
            {
                const auto* identity = m_world.ecs().tryGet<RoleIdentity>(event.entity);
                if (identity && identity->roleType != static_cast<int32_t>(mugen::EntityRoleType::kHero))
                    m_lastHurtTarget = event.entity;
            }
            else if (std::holds_alternative<Victory>(event.payload))
                m_roomMessage = "Victory";
            else if (std::holds_alternative<Failure>(event.payload))
                m_roomMessage = "Defeat";
            else if (const auto* warning = std::get_if<RoomWarning>(&event.payload))
            {
                m_roomMessage = warning->m_boss ? "Boss incoming" : "Warning";
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
    m_presenter->presenter.present(m_world, m_accumulator * 30.0f, delta);
    if (m_autotestFrames > 0 &&
        m_world.ecs().singleton<mugen::sim::WorldClock>().frame >= static_cast<uint64_t>(m_autotestFrames))
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
        std::string error;
        if (!m_replay.save(m_replayPath, error))
            AXLOGE("LocalBattleMode: replay save failed: {}", error);
    }
    m_presenter.reset();
}

void LocalBattleMode::renderDebugPanel()
{
    if (m_initialized)
    {
        auto& debugDraw     = m_presenter->presenter.debugDraw();
        bool collisionDebug = debugDraw.enabled();
        m_debugPanel.render(m_world, m_debugPaused, m_debugSingleStep, collisionDebug);
        debugDraw.setEnabled(collisionDebug);

        const auto& room = m_world.ecs().singleton<mugen::sim::RoomState>();
        ImGui::Begin("Room");
        ImGui::TextUnformatted(m_roomMessage.c_str());
        ImGui::Text("State %d", static_cast<int>(room.m_state));
        ImGui::Text("Wave %d / %d", room.m_wave, static_cast<int>(room.m_config.battleRuleParam.size()));
        ImGui::Text("Revives %d / %d", room.m_usedRevives, room.m_maxRevives);
        if (room.m_state == mugen::RoomState::ReadySpine && room.m_pending.empty())
            if (ImGui::Button("Continue"))
                m_systemCommands.push_back(mugen::sim::RoomWarningDone{m_roomId});
        ImGui::BeginDisabled(!room.m_reviving || room.m_failure);
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
    auto& debugDraw = m_presenter->presenter.debugDraw();
    debugDraw.setEnabled(!debugDraw.enabled());
}

void LocalBattleMode::setInput(uint32_t slot, bool pressed)
{
    if (slot < static_cast<uint32_t>(mugen::sim::ControlKey::Count))
        m_control.setKey(static_cast<mugen::sim::ControlKey>(slot), pressed);
}

}  // namespace gameui
