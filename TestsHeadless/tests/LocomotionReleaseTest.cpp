#include "doctest.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/status/Status.h"

using namespace mugen;
using namespace mugen::sim;

namespace
{
// 生成可控角色，并放开移动范围，避免碰到地图边缘被误判为停止。
Entity setupHero(World& world)
{
    REQUIRE(conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"));
    world.init({});
    const auto& role = conf::Config::instance().get<conf::RoleConfig>(104);
    REQUIRE(role.velocity > Fixed{});
    RoleSpawnParams params;
    params.roleId                                 = role.id;
    params.level                                  = 100;
    params.playerSlot                             = 0;
    const Entity entity                           = spawnRole(world, params);
    world.ecs().singleton<MapRegion>().moveRanges = {{"-10000"_fx, "-10000"_fx, "20000"_fx, "20000"_fx}};
    return entity;
}

// 把本帧按键状态推进为一帧逻辑。
void step(World& world, ControlManagerCore& control)
{
    std::vector<PlayerCommand> commands;
    control.advance(commands);
    FrameInput input;
    input.frame = world.ecs().singleton<WorldClock>().frame + 1;
    if (!commands.empty())
        input.players.push_back({0, std::move(commands)});
    world.tick(input);
}
}  // namespace

TEST_CASE("Locomotion held direction key keeps the hero moving")
{
    World world;
    const Entity entity = setupHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Right, true);
    const Fixed start = world.ecs().get<Transform>(entity).position.x;
    for (int frame = 0; frame < 6; ++frame)
        step(world, control);
    CHECK(world.ecs().get<Transform>(entity).position.x > start);
    CHECK(status::isRunStatus(world.ecs().get<RoleStatus>(entity)));
}

TEST_CASE("Locomotion direction key release stops within two frames and enters Idle")
{
    World world;
    const Entity entity = setupHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Right, true);
    const Fixed start = world.ecs().get<Transform>(entity).position.x;
    for (int frame = 0; frame < 6; ++frame)
        step(world, control);
    REQUIRE(world.ecs().get<Transform>(entity).position.x > start);

    control.setKey(ControlKey::Right, false);
    step(world, control);
    step(world, control);
    const auto settled = world.ecs().get<Transform>(entity).position;
    const auto& role   = world.ecs().get<RoleStatus>(entity);
    CHECK(status::isIdleStatus(role));
    CHECK_FALSE(status::isRunStatus(role));
    CHECK_FALSE(status::isWalkStatus(role));
    step(world, control);
    const auto& stopped = world.ecs().get<Transform>(entity).position;
    CHECK(stopped.x == settled.x);
    CHECK(stopped.z == settled.z);
}
