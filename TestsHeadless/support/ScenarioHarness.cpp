#include "ScenarioHarness.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/io/FileUtils.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/hit/Hit.h"
#include <filesystem>

namespace mugen::test
{
namespace
{
void recordedRole(sim::World& world, Entity entity)
{
    auto& ecs = world.ecs();
    if (!ecs.get<sim::RoleIdentity>(entity).playerSlot)
        return;
    for (const auto& command : ecs.get<sim::ControlEventQueue>(entity).commands)
        if (command.controlEvent == ControlEvent::CTR_ATKA)
            ecs.each<sim::RoleIdentity, sim::Vitals>(
                [&](Entity target, const sim::RoleIdentity& role, sim::Vitals& vitals) {
                    if (role.roleType >= 2 && role.roleType <= 4)
                    {
                        sim::hit::modifyHp(world, target, -vitals.hp, {}, true);
                        world.destroy(target);
                    }
                });
        else if (command.controlEvent == ControlEvent::CTR_RUN)
            ecs.each<sim::PortalState, sim::Transform>([&](Entity, const sim::PortalState& portal,
                                                            const sim::Transform& transform) {
                if (portal.m_phase == sim::PortalPhase::Active)
                    ecs.get<sim::Transform>(entity).position = transform.position;
            });
    ecs.get<sim::ControlEventQueue>(entity).commands.clear();
}
}  // namespace
void ScenarioHarness::loadAssets()
{
    const auto content = std::filesystem::path(MUGEN_CONTENT_DIR);
    io::addSearchPath(content.string());
    auto& config = conf::Config::instance();
    if (!config.loaded() && !config.load((content / "mugen/config/config.bin").string()))
    {
        MG_LOG_E("Cannot load real config.bin");
        return;
    }
    auto& avatar = *AvatarAssetCache::getInstance();
    avatar.addSearchPath("res_zhcn");
    if (!avatar.isLoaded() && !avatar.load((content / "mugen/config/avatar.bin").string()))
    {
        MG_LOG_E("Cannot load real avatar.bin");
        return;
    }
}
sim::WorldSetup ScenarioHarness::roomSetup(int32_t roomId, uint64_t seed)
{
    loadAssets();
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(roomId);
    if (!room)
    {
        MG_LOG_E("Room config not found: {}", roomId);
        return {};
    }
    return {seed, sim::WorldMode::Battle, roomId, room->mapKey};
}
sim::World ScenarioHarness::roomWorld(int32_t roomId, uint64_t seed)
{
    const auto setup = roomSetup(roomId, seed);
    sim::World world(roomHooks());
    world.init(setup);
    sim::RoleSpawnParams player;
    player.roleId     = 0;
    player.camp       = 2;
    player.playerSlot = 0;
    spawnRole(world, player);
    const auto* config = conf::Config::instance().find<conf::RoomConfig>(roomId);
    if (!config || !sim::room::enter(world, *config, 0, 0))
        MG_LOG_E("Cannot enter recorded room {}", roomId);
    return world;
}
sim::WorldHooks ScenarioHarness::roomHooks()
{
    sim::WorldHooks hooks;
    hooks.update[0] = recordedRole;
    return hooks;
}
ScenarioHarness::ScenarioHarness(sim::WorldSetup setup, sim::WorldHooks hooks) : m_world(hooks)
{
    loadAssets();
    m_world.init(std::move(setup));
}
bool ScenarioHarness::inject(uint64_t frame, uint32_t slot, sim::PlayerCommand command)
{
    if (frame <= m_world.ecs().singleton<sim::WorldClock>().frame)
    {
        MG_LOG_E("Cannot inject input into an elapsed frame");
        return false;
    }
    auto& input = m_inputs[frame];
    input.frame = frame;
    input.players.push_back({slot, {command}});
    return true;
}
bool ScenarioHarness::injectSystem(uint64_t frame, sim::SystemCommand command)
{
    if (frame <= m_world.ecs().singleton<sim::WorldClock>().frame)
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
    const uint64_t frame = m_world.ecs().singleton<sim::WorldClock>().frame + 1;
    auto input           = m_inputs.extract(frame);
    m_world.tick(input.empty() ? sim::FrameInput{frame, {}, {}, {}} : input.mapped());
}
void ScenarioHarness::runTo(uint64_t frame)
{
    while (m_world.ecs().singleton<sim::WorldClock>().frame < frame)
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
