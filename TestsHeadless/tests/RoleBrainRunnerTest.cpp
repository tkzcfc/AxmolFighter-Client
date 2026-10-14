#include "doctest.h"
#include "SkillTestSupport.h"
#include "../support/ScenarioHarness.h"
#include "mugen/sim/role/brain/RoleBrain.h"
#include "mugen/sim/role/brain/RoleBrainRunner.h"
#include "mugen/sim/role/brain/RoleContext.h"
#include "mugen/sim/role/brain/RoleBranches.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/sim/role/Role.h"

using namespace mugen;
using namespace mugen::sim;

namespace
{
// 建立一个战斗角色，脑与控制作为本地副本独立驱动（不在 ECS 注册）。
struct BrainFixture
{
    World world;
    Entity entity;
    role::brain::RoleBrain brain;
    role::brain::RoleControl control;

    explicit BrainFixture()
    {
        test::ScenarioHarness::loadAssets();
        world.init(test::ScenarioHarness::roomSetup(2071501, 31));
        sim::RoleSpawnParams p;
        p.camp       = 2;
        p.playerSlot = 0;
        entity       = sim::spawnPlayer(world, CharacterClass::kSwordman, p);
        REQUIRE(entity.value != 0);
    }
};
}  // namespace

TEST_CASE("RoleBrain standalone first enter picks Idle and updates once")
{
    BrainFixture f;
    REQUIRE(role::brain::enterFirst(f.brain, f.world, f.entity, false));
    CHECK(f.brain.m_phase == role::brain::BrainPhase::Running);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
    CHECK(std::get<behavior::ActionState>(f.brain.m_data).fTime == kLogicDtMs);
}

TEST_CASE("RoleBrain standalone selects cyclically Run then Idle")
{
    BrainFixture f;
    REQUIRE(role::brain::enterFirst(f.brain, f.world, f.entity, false));
    Role(f.world, f.entity).changeStatus(EntityRoleStatus::Run);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Run);
    Role(f.world, f.entity).changeStatus(EntityRoleStatus::Idle);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
}

TEST_CASE("RoleBrain with no enterable branch falls back to Idle")
{
    BrainFixture f;
    REQUIRE(role::brain::enterFirst(f.brain, f.world, f.entity, false));
    // 无任何状态位时所有守卫都不成立，回退到待机分支。
    f.world.ecs().get<RoleStatus>(f.entity).eEntityStatus = EntityRoleStatus::None;
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
    CHECK(f.brain.m_phase == role::brain::BrainPhase::Running);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
    CHECK(f.brain.m_phase == role::brain::BrainPhase::Running);
}

TEST_CASE("RoleBrain branch selection follows movement commands")
{
    BrainFixture f;
    REQUIRE(role::brain::enterFirst(f.brain, f.world, f.entity, false));
    // 行走命令进入 Walk 分支。
    PlayerCommand walk;
    walk.controlEvent = ControlEvent::CTR_WALK;
    walk.quadrant     = 3;
    f.world.ecs().get<ControlEventQueue>(f.entity).commands.push_back(walk);
    role::brain::RoleContext(f.world, f.entity).consumeControl();
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Walk);
    // 待机命令回到 Idle 分支。
    PlayerCommand idle;
    idle.controlEvent = ControlEvent::CTR_IDLE;
    f.world.ecs().get<ControlEventQueue>(f.entity).commands.push_back(idle);
    role::brain::RoleContext(f.world, f.entity).consumeControl();
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
    // 跑动命令进入 Run 分支。
    PlayerCommand run;
    run.controlEvent = ControlEvent::CTR_RUN;
    run.quadrant     = 3;
    f.world.ecs().get<ControlEventQueue>(f.entity).commands.push_back(run);
    role::brain::RoleContext(f.world, f.entity).consumeControl();
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Run);
}
