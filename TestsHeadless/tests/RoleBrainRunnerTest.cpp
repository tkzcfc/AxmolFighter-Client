#include "doctest.h"
#include "SkillTestSupport.h"
#include "../support/ScenarioHarness.h"
#include "mugen/sim/role/brain/RoleBrain.h"
#include "mugen/sim/role/brain/RoleBrainRunner.h"
#include "mugen/sim/role/brain/RoleBranches.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/sim/status/Status.h"

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
        brain.m_attackLayout = behavior::definitionKey(world, entity);
    }
    // 返回 RoleTree 当前运行叶子的动作类型（用于对照）。
    behavior::ActionType treeAction() const
    {
        auto& tree      = world.ecs().get<RoleTree>(entity);
        const auto& def = behavior::definition(tree.defKey);
        for (uint32_t i = 0; i < tree.instance.nodes.size(); ++i)
            if (def.topology().node(i).type == BTNodeType::Action && tree.instance.nodes[i].status == BTStatus::Running)
                if (const auto* t = std::get_if<behavior::ActionType>(&def.param(def.topology().node(i).paramIndex)))
                    return *t;
        return behavior::ActionType::None;
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
    status::dealWithStatus(f.world, f.entity, EntityRoleStatus::Run);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Run);
    status::dealWithStatus(f.world, f.entity, EntityRoleStatus::Idle);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_branch == role::brain::RoleBranch::Idle);
}

TEST_CASE("RoleBrain with no enterable branch stays stopped permanently")
{
    BrainFixture f;
    REQUIRE(role::brain::enterFirst(f.brain, f.world, f.entity, false));
    // 无任何状态位时所有守卫都不成立。
    f.world.ecs().get<RoleStatus>(f.entity).eEntityStatus = EntityRoleStatus::None;
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_phase == role::brain::BrainPhase::Stopped);
    role::brain::update(f.brain, f.world, f.entity, false);
    CHECK(f.brain.m_phase == role::brain::BrainPhase::Stopped);
}

TEST_CASE("RoleBrain branch selection matches RoleTree over scripted frames")
{
    // 两个世界跑相同移动脚本，比较每帧当前分支动作类型。
    BrainFixture treeFixt;
    BrainFixture brainFixt;
    REQUIRE(role::brain::enterFirst(brainFixt.brain, brainFixt.world, brainFixt.entity, false));
    for (uint64_t i = 1; i <= 60; ++i)
    {
        // 脚本：纯移动（行走/跑动/待机），避免战斗副作用使两套管线分叉。
        PlayerCommand command;
        command.controlEvent = i % 40 < 20 ? ControlEvent::CTR_WALK : ControlEvent::CTR_RUN;
        command.quadrant     = 3;
        command.pressType    = 1;
        // RoleTree 路径：叶子级推进（消费控制 + 行为树更新）。
        treeFixt.world.ecs().get<ControlEventQueue>(treeFixt.entity).commands.push_back(command);
        behavior::consumeControl(treeFixt.world, treeFixt.entity);
        behavior::update(treeFixt.world, treeFixt.entity);
        // RoleBrain 路径：同样叶子级推进。
        brainFixt.world.ecs().get<ControlEventQueue>(brainFixt.entity).commands.push_back(command);
        behavior::consumeControl(brainFixt.world, brainFixt.entity);
        role::brain::update(brainFixt.brain, brainFixt.world, brainFixt.entity, false);
        const auto treeAct  = treeFixt.treeAction();
        const auto brainAct = role::brain::branchAction(brainFixt.brain.m_branch);
        INFO("frame=", i, " tree=", static_cast<int>(treeAct), " brain=", static_cast<int>(brainAct));
        CHECK(treeAct == brainAct);
    }
}
