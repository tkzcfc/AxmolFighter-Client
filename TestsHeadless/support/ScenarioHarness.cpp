#include "ScenarioHarness.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/conf/ReflectedConfig.h"
#include "ContentVfs.h"
#include "mugen/core/io/FileUtils.h"
#include "mugen/sim_legacy/BattleBootstrap.h"
#include "mugen/sim_legacy/role/Role.h"
#include <filesystem>

namespace mugen::test
{
namespace
{
void recordedRole(sim_legacy::World& world, Entity entity)
{
    auto& ecs = world.ecs();
    if (!ecs.get<sim_legacy::RoleIdentity>(entity).playerSlot)
        return;
    for (const auto& command : ecs.get<sim_legacy::ControlEventQueue>(entity).commands)
        if (command.controlEvent == ControlEvent::CTR_ATKA)
            ecs.each<sim_legacy::RoleIdentity, sim_legacy::Vitals>(
                [&](Entity target, const sim_legacy::RoleIdentity& role, sim_legacy::Vitals& vitals) {
                    if (role.roleType >= 2 && role.roleType <= 4)
                    {
                        sim_legacy::Role(world, target).modifyHp(-vitals.hp, {}, true);
                        world.destroy(target);
                    }
                });
        else if (command.controlEvent == ControlEvent::CTR_RUN)
            ecs.each<sim_legacy::PortalState, sim_legacy::Transform>([&](Entity, const sim_legacy::PortalState& portal,
                                                            const sim_legacy::Transform& transform) {
                if (portal.m_phase == sim_legacy::PortalPhase::Active)
                    ecs.get<sim_legacy::Transform>(entity).position = transform.position;
            });
    ecs.get<sim_legacy::ControlEventQueue>(entity).commands.clear();
}
}  // namespace
void ScenarioHarness::loadAssets()
{
    if (!ensureContentVfs())
    {
        MG_LOG_E("Cannot mount Content VFS");
        return;
    }
    auto& config = conf::Config::instance();
    if (!config.loaded() && !config.load("/mugen/config/config.bin"))
    {
        MG_LOG_E("Cannot load real config.bin");
        return;
    }
    auto& avatar = *AvatarAssetCache::getInstance();
    if (!avatar.isLoaded() && !avatar.load("/mugen/config/avatar.bin"))
    {
        MG_LOG_E("Cannot load real avatar.bin");
        return;
    }
}
sim_legacy::WorldSetup ScenarioHarness::roomSetup(int32_t roomId, uint64_t seed)
{
    loadAssets();
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(roomId);
    if (!room)
    {
        MG_LOG_E("Room config not found: {}", roomId);
        return {};
    }
    return {seed, sim_legacy::WorldMode::Battle, roomId, room->mapKey};
}
sim_legacy::World ScenarioHarness::roomWorld(int32_t roomId, uint64_t seed)
{
    auto setup               = roomSetup(roomId, seed);
    sim_legacy::PlayerSetup player;
    player.m_roleId          = 0;
    player.m_camp            = 2;
    player.m_slot            = 0;
    setup.m_players.push_back(player);
    setup.m_stageId          = 0;
    setup.m_copyId           = 0;
    setup.m_useScenarioHooks = true;
    sim_legacy::World world(roomHooks());
    if (sim_legacy::bootstrapBattle(world, setup).empty())
        MG_LOG_E("Cannot enter recorded room {}", roomId);
    return world;
}
sim_legacy::WorldHooks ScenarioHarness::roomHooks()
{
    sim_legacy::WorldHooks hooks;
    hooks.update[0] = recordedRole;
    return hooks;
}
ScenarioHarness::ScenarioHarness(sim_legacy::WorldSetup setup, sim_legacy::WorldHooks hooks) : m_world(hooks)
{
    loadAssets();
    m_world.init(std::move(setup));
}
bool ScenarioHarness::inject(uint64_t frame, uint32_t slot, sim_legacy::PlayerCommand command)
{
    if (frame <= m_world.ecs().singleton<sim_legacy::WorldClock>().frame)
    {
        MG_LOG_E("Cannot inject input into an elapsed frame");
        return false;
    }
    auto& input = m_inputs[frame];
    input.frame = frame;
    input.players.push_back({slot, {command}});
    return true;
}
bool ScenarioHarness::injectSystem(uint64_t frame, sim_legacy::SystemCommand command)
{
    if (frame <= m_world.ecs().singleton<sim_legacy::WorldClock>().frame)
    {
        MG_LOG_E("Cannot inject input into an elapsed frame");
        return false;
    }
    auto& input = m_inputs[frame];
    input.frame = frame;
    input.systemCommands.push_back(std::move(command));
    return true;
}
void ScenarioHarness::tick()
{
    const uint64_t frame = m_world.ecs().singleton<sim_legacy::WorldClock>().frame + 1;
    auto input           = m_inputs.extract(frame);
    m_world.tick(input.empty() ? sim_legacy::FrameInput{frame, {}, {}, {}} : input.mapped());
}
void ScenarioHarness::runTo(uint64_t frame)
{
    while (m_world.ecs().singleton<sim_legacy::WorldClock>().frame < frame)
        tick();
}
size_t ScenarioHarness::entityCount()
{
    size_t count = 0;
    m_world.ecs().each<>([&](Entity) { ++count; });
    return count;
}
bool ScenarioHarness::expectEntityCount(size_t expected)
{
    return entityCount() == expected;
}
bool ScenarioHarness::expectHash(uint64_t expected) const
{
    return m_world.hash() == expected;
}
}  // namespace mugen::test
