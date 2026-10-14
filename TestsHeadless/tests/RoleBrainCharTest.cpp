#include "doctest.h"
#include "BTTestTypes.h"
#include "SkillTestSupport.h"
#include "../support/ScenarioHarness.h"
#include "mugen/sim/attack/AttackRole.h"
#include "mugen/sim/role/brain/states/HitStates.h"
#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/conf/ReflectedConfig.h"
#include <fstream>

using namespace mugen;
using namespace mugen::sim;

// 表征测试：固定当前旧 BT 代码的行为，供后续状态机重构做行为等价对照。
namespace
{
// 查找第一个 Running 的指定叶子类型节点，返回节点 id（-1 未找到）。
int32_t runningLeaf(const RoleTree& tree, const behavior::Def& def, behavior::Leaf leafType)
{
    for (uint32_t i = 0; i < tree.instance.nodes.size(); ++i)
        if (def.topology().node(i).type == BTNodeType::Action && def.topology().node(i).leafTypeId == leafType &&
            tree.instance.nodes[i].status == BTStatus::Running)
            return static_cast<int32_t>(i);
    return -1;
}
}  // namespace

TEST_CASE("Root selector picks next branch cyclically skipping the finished one")
{
    using namespace bt_test;
    const Def def({composite(BTNodeType::Selector, 3), action(0), action(1), action(2)}, {}, params());
    Instance instance(def);
    Ctx ctx{instance};
    Runner runner(def, registry, instance);
    REQUIRE(runner.enter(ctx));
    CHECK(instance.nodes[0].currentIndex == 0);
    CHECK(ctx.log == Log{"A0.enter"});
    ctx.results[0] = BTStatus::Success;
    REQUIRE(runner.update(ctx));
    CHECK(instance.nodes[0].currentIndex == 1);
    ctx.results[1] = BTStatus::Success;
    REQUIRE(runner.update(ctx));
    CHECK(instance.nodes[0].currentIndex == 2);
    ctx.results[2] = BTStatus::Success;
    REQUIRE(runner.update(ctx));
    CHECK(instance.nodes[0].currentIndex == 0);
    CHECK(ctx.log == Log{"A0.enter", "A0.update:1", "A0.exit", "A1.enter", "A1.update:1", "A1.exit", "A2.enter",
                         "A2.update:1", "A2.exit", "A0.enter"});
}

TEST_CASE("Root with no enterable branch stops permanently")
{
    using namespace bt_test;
    const Def def({composite(BTNodeType::Selector, 1, {0, 1}), action(0)}, {{0, 0}}, params());
    Instance instance(def);
    Ctx ctx{instance};
    ctx.gates[0] = false;
    Runner runner(def, registry, instance);
    StderrCapture capture;
    CHECK_FALSE(runner.enter(ctx));
    CHECK(instance.nodes[0].status == BTStatus::Failure);
    CHECK_FALSE(runner.update(ctx));
    CHECK(instance.nodes[0].status == BTStatus::Failure);
}

TEST_CASE("Attack leaf enter preserves static timer left by previous run of same node")
{
    skill_test::loadConfig();
    const auto id = skill_test::findSkill([](const auto& s) {
        return !s.actionIds.empty() && !s.actionIds.front().empty() &&
               conf::Config::instance().find<conf::ActionConfig>(s.actionIds.front().front());
    });
    const auto& row = conf::Config::instance().get<conf::ActionConfig>(
        conf::Config::instance().get<conf::SkillAttackConfig>(id).actionIds.front().front());
    World world;
    const auto entity = skill_test::actor(world);
    skill_test::slots(world, entity, {id});
    world.ecs().add(entity, MotionState{});
    world.ecs().add(entity, RoleHitState{});
    world.ecs().add(entity, RoleTimers{});
    world.ecs().add(entity, Displacement{});
    world.ecs().add(entity, ColliderState{});
    RoleTree tree;
    tree.defKey = behavior::definitionKey(world, entity);
    world.ecs().add(entity, tree);
    const behavior::Param param = behavior::AttackAction{row.id, 1, 0};
    behavior::LeafState state   = attack::AttackRole{};
    role::brain::RoleContext rc{world, entity};
    attack::AttackAction::enter(rc, std::get<behavior::AttackAction>(param), std::get<attack::AttackRole>(state));
    auto& leaf                        = std::get<attack::AttackRole>(state);
    leaf.m_static                    = true;
    leaf.m_staticResetAt             = "123"_fx;
    attack::AttackAction::exit(rc, std::get<behavior::AttackAction>(param), std::get<attack::AttackRole>(state));
    attack::AttackAction::enter(rc, std::get<behavior::AttackAction>(param), std::get<attack::AttackRole>(state));
    const auto& again = std::get<attack::AttackRole>(state);
    CHECK(again.m_static);
    CHECK(again.m_staticResetAt == "123"_fx);
    conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin");
}

TEST_CASE("First frame after spawn only enters, first tick updates once")
{
    test::ScenarioHarness::loadAssets();
    World world;
    world.init(test::ScenarioHarness::roomSetup(2071501, 31));
    sim::RoleSpawnParams player;
    player.camp       = 2;
    player.playerSlot = 0;
    const auto entity = sim::spawnPlayer(world, CharacterClass::kSwordman, player);
    REQUIRE(entity.value != 0);
    auto& tree        = world.ecs().get<RoleTree>(entity);
    const auto& def   = behavior::definition(tree.defKey);
    REQUIRE_FALSE(tree.instance.nodes.empty());
    const auto idleNode = runningLeaf(tree, def, behavior::kIdleLeaf);
    REQUIRE(idleNode >= 0);
    const auto leafIdx = def.topology().leafIndex(static_cast<uint32_t>(idleNode));
    CHECK(std::get<behavior::ActionState>(tree.instance.leaves[leafIdx]).fTime == Fixed{});
    world.tick({1, {}, {}, {}});
    CHECK(std::get<behavior::ActionState>(tree.instance.leaves[leafIdx]).fTime == kLogicDtMs);
}

TEST_CASE("Hit state exit resets displacement only for Death and landing Hit")
{
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    skill::loadSkill(world, entity, skill::playerLoadout(skill::offlineKit(CharacterClass::kSwordman)));
    world.ecs().add(entity, MotionState{});
    world.ecs().add(entity, RoleHitState{});
    world.ecs().add(entity, RoleTimers{});
    world.ecs().add(entity, Displacement{});
    world.ecs().add(entity, ColliderState{});
    RoleTree tree;
    tree.defKey   = behavior::definitionKey(world, entity);
    world.ecs().add(entity, tree);
    auto& rt      = world.ecs().get<RoleTree>(entity);
    rt.instance   = BTInstance<behavior::LeafState>(behavior::definition(rt.defKey));
    auto& disp    = world.ecs().get<Displacement>(entity);
    world.ecs().get<Transform>(entity).position.y = Fixed{};
    const conf::SkillHitConfig* hitRow = nullptr;
    for (const auto& h : conf::Config::instance().table<conf::SkillHitConfig>())
        if (h.hitType == 0)
        {
            hitRow = &h;
            break;
        }
    REQUIRE(hitRow != nullptr);
    role::brain::RoleContext rc{world, entity};
    {  // Hit (landing) -> exit resets displacement
        behavior::LeafState state = behavior::HitActionState{};
        role::brain::states::HitState::enter(rc, behavior::ActionType::Hit, std::get<behavior::HitActionState>(state));
        std::get<behavior::HitActionState>(state).m_hitId = hitRow->id;
        disp.listenerNode                               = 7;
        role::brain::states::HitState::exit(rc, behavior::ActionType::Hit, std::get<behavior::HitActionState>(state));
        CHECK(disp.listenerNode == -1);
    }
    {  // HitUp -> exit does not reset displacement
        behavior::LeafState state = behavior::HitActionState{};
        role::brain::states::HitState::enter(rc, behavior::ActionType::HitUp, std::get<behavior::HitActionState>(state));
        disp.listenerNode = 7;
        role::brain::states::HitState::exit(rc, behavior::ActionType::HitUp, std::get<behavior::HitActionState>(state));
        CHECK(disp.listenerNode == 7);
    }
    conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin");
}
