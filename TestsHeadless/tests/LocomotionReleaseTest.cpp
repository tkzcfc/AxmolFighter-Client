#include "doctest.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/spawn/RoleSpawn.h"

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
    CHECK(world.ecs().get<RoleStatus>(entity).isRun());
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
    CHECK(role.isIdle());
    CHECK_FALSE(role.isRun());
    CHECK_FALSE(role.isWalk());
    step(world, control);
    const auto& stopped = world.ecs().get<Transform>(entity).position;
    CHECK(stopped.x == settled.x);
    CHECK(stopped.z == settled.z);
}

TEST_CASE("Locomotion Control diagonal Up and Right moves positive x and z")
{
    World world;
    const Entity entity = setupHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Up, true);
    control.setKey(ControlKey::Right, true);
    const auto start = world.ecs().get<Transform>(entity).position;
    for (int frame = 0; frame < 6; ++frame)
        step(world, control);
    const auto after = world.ecs().get<Transform>(entity).position;
    CHECK(after.x > start.x);
    CHECK(after.z > start.z);
    CHECK(world.ecs().get<RoleStatus>(entity).isRun());
}

TEST_CASE("Locomotion Control Right then Up keeps moving on both axes")
{
    World world;
    const Entity entity = setupHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Right, true);
    for (int frame = 0; frame < 4; ++frame)
        step(world, control);
    const auto beforeUp = world.ecs().get<Transform>(entity).position;
    control.setKey(ControlKey::Up, true);
    for (int frame = 0; frame < 4; ++frame)
        step(world, control);
    const auto after = world.ecs().get<Transform>(entity).position;
    CHECK(after.x > beforeUp.x);
    CHECK(after.z > beforeUp.z);
}

TEST_CASE("Locomotion Control Left then Right while both held moves positive x")
{
    World world;
    const Entity entity = setupHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Left, true);
    step(world, control);
    const auto turned = world.ecs().get<Transform>(entity).position.x;
    control.setKey(ControlKey::Right, true);
    for (int frame = 0; frame < 4; ++frame)
        step(world, control);
    CHECK(world.ecs().get<Transform>(entity).position.x > turned);
    CHECK(world.ecs().get<RoleStatus>(entity).isRun());
}

TEST_CASE("Locomotion Control releasing Right while Left stays held moves negative x")
{
    World world;
    const Entity entity = setupHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Left, true);
    step(world, control);
    control.setKey(ControlKey::Right, true);
    for (int frame = 0; frame < 4; ++frame)
        step(world, control);
    const auto beforeRelease = world.ecs().get<Transform>(entity).position.x;
    control.setKey(ControlKey::Right, false);
    bool keptRunning = true;
    for (int frame = 0; frame < 2; ++frame)
    {
        step(world, control);
        keptRunning = keptRunning && world.ecs().get<RoleStatus>(entity).isRun();
    }
    CHECK(world.ecs().get<Transform>(entity).position.x < beforeRelease);
    CHECK(keptRunning);
}
