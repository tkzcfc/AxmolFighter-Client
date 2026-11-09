#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim_legacy/input/ControlManagerCore.h"
#include "mugen/sim_legacy/role/brain/RoleBrain.h"
#include "mugen/sim_legacy/role/brain/RoleBrainDriver.h"
#include "mugen/sim_legacy/spawn/RoleSpawn.h"

using namespace mugen;
using namespace mugen::sim_legacy;

namespace
{
// 生成城镇模式可控角色，并放开移动范围避免撞到地图边缘。
Entity setupTownHero(World& world)
{
    REQUIRE((mugen::test::ensureContentVfs() && conf::Config::instance().load("/mugen/config/config.bin")));
    world.init({1, WorldMode::Town, 41, {}});
    const auto& role = conf::Config::instance().get<conf::RoleConfig>(104);
    REQUIRE(role.velocity > Fixed{});
    RoleSpawnParams params;
    params.roleId     = role.id;
    params.level      = 100;
    params.playerSlot = 0;
    const Entity entity = RoleSpawner(world).spawnRole(params);
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

TEST_CASE("Town controllable role runs via keyboard and returns to Idle on release")
{
    World world;
    const Entity entity = setupTownHero(world);
    ControlManagerCore control;
    control.setKey(ControlKey::Right, true);
    const Fixed start = world.ecs().get<Transform>(entity).position.x;
    for (int frame = 0; frame < 6; ++frame)
        step(world, control);

    // 按住方向键后角色进入跑动分支，位置沿 X 正向前进，不再被强制退回待机。
    CHECK(world.ecs().get<Transform>(entity).position.x > start);
    CHECK(world.ecs().get<RoleStatus>(entity).isRun());
    CHECK(world.ecs().get<RoleBrain>(entity).m_branch == role::brain::RoleBranch::Run);
    CHECK(world.ecs().get<MotionState>(entity).motionName == "walk");

    // 松开方向键后回到待机并停止。
    control.setKey(ControlKey::Right, false);
    step(world, control);
    step(world, control);
    const auto settled = world.ecs().get<Transform>(entity).position;
    CHECK(world.ecs().get<RoleStatus>(entity).isIdle());
    CHECK_FALSE(world.ecs().get<RoleStatus>(entity).isRun());
    CHECK_FALSE(world.ecs().get<RoleStatus>(entity).isWalk());
    CHECK(world.ecs().get<RoleBrain>(entity).m_branch == role::brain::RoleBranch::Idle);
    step(world, control);
    CHECK(world.ecs().get<Transform>(entity).position.x == settled.x);
    CHECK(world.ecs().get<Transform>(entity).position.z == settled.z);
}

TEST_CASE("Town controllable role walks via CTR_WALK and stops on CTR_IDLE")
{
    World world;
    const Entity entity = setupTownHero(world);
    const Fixed start   = world.ecs().get<Transform>(entity).position.x;

    FrameInput walkInput;
    walkInput.frame = 1;
    walkInput.players.push_back({0, {{ControlEvent::CTR_WALK, ControlQuadrant::UpRight, 0, 0}}});
    world.tick(walkInput);
    for (int frame = 0; frame < 5; ++frame)
        world.tick({world.ecs().singleton<WorldClock>().frame + 1, {}, {}, {}});

    CHECK(world.ecs().get<Transform>(entity).position.x > start);
    CHECK(world.ecs().get<RoleStatus>(entity).isWalk());
    CHECK(world.ecs().get<RoleBrain>(entity).m_branch == role::brain::RoleBranch::Walk);
    CHECK(world.ecs().get<MotionState>(entity).motionName == "running");

    FrameInput idleInput;
    idleInput.frame = world.ecs().singleton<WorldClock>().frame + 1;
    idleInput.players.push_back({0, {{ControlEvent::CTR_IDLE, ControlQuadrant::None, 0, 0}}});
    world.tick(idleInput);
    CHECK(world.ecs().get<RoleStatus>(entity).isIdle());
    CHECK(world.ecs().get<RoleBrain>(entity).m_branch == role::brain::RoleBranch::Idle);
}
