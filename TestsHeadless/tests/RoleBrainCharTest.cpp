#include "doctest.h"
#include "BTTestTypes.h"
#include "SkillTestSupport.h"
#include "../support/ScenarioHarness.h"
#include "mugen/sim/attack/AttackRole.h"
#include "mugen/sim/role/brain/states/HitStates.h"
#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/conf/ReflectedConfig.h"
#include <fstream>
#include <sstream>
#include <vector>

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

TEST_CASE("Interrupt window pins per-frame skill address and pipe timeline")
{
    // 找一个带 nextSkill 链且可普通打断的技能，驱动真实 World 施放与续接。
    skill_test::loadConfig();
    auto* assets = AvatarAssetCache::getInstance();
    assets->addSearchPath("res_zhcn");
    REQUIRE(assets->load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/avatar.bin"));
    int32_t skillId = -1;
    conf::ActionConfig action;
    std::string motionFile;
    for (const auto& skill : conf::Config::instance().table<conf::SkillAttackConfig>())
    {
        if (skill.cd != 0 || skill.actionIds.empty() || skill.actionIds.front().empty() || skill.nextSkill == -1)
            continue;
        const auto* row = conf::Config::instance().find<conf::ActionConfig>(skill.actionIds.front().front());
        if (!row || row->interruptFrame < 1)
            continue;
        const auto name = std::holds_alternative<std::string>(row->action)
                              ? std::get<std::string>(row->action)
                              : std::to_string(std::get<int32_t>(row->action));
        for (const auto& [path, map] : assets->motionMaps)
        {
            if (path.find("mugen/motion/hero/") == std::string::npos)
                continue;
            if (map.findMotion(name) && map.findMotion(name)->durationMs() > 0)
            {
                skillId = skill.id;
                action = *row;
                motionFile = path;
                break;
            }
        }
        if (skillId != -1)
            break;
    }
    REQUIRE(skillId != -1);
    World world;
    const auto entity = skill_test::actor(world);
    skill_test::slots(world, entity, {skillId});
    auto& ecs = world.ecs();
    MotionState motion;
    motion.motionFile = motionFile;
    ecs.add(entity, motion);
    ecs.add(entity, RoleHitState{});
    ecs.add(entity, RoleTimers{});
    ecs.add(entity, Displacement{});
    ecs.add(entity, ColliderState{});
    ecs.add(entity, ControlEventQueue{});
    RoleTree tree;
    tree.defKey = behavior::definitionKey(world, entity);
    ecs.add(entity, tree);
    ecs.singleton<PlayerSlots>().entities[0] = entity;
    // 每帧记录 (当前技能 configId.step, 管线, 上次施放技能 configId)。
    auto key = [&]() {
        auto& pool = ecs.get<SkillPool>(entity);
        const std::string cur = pool.m_current == kNoSkill ? "-" :
            std::to_string(pool.m_skills[pool.m_current].m_configId) + "." + std::to_string(pool.m_skills[pool.m_current].m_address.m_step);
        const std::string prev = pool.m_previous == kNoSkill ? "-" : std::to_string(pool.m_skills[pool.m_previous].m_configId);
        return cur + ".p" + std::to_string(pool.m_skills.empty() ? -1 : (pool.m_current == kNoSkill ? -1 : pool.m_skills[pool.m_current].m_pipe)) +
               ".prev" + prev;
    };
    std::vector<std::string> timeline;
    uint64_t frame = 0;
    auto tick = [&](bool press) {
        FrameInput input;
        input.frame = ++frame;
        if (press)
        {
            PlayerCommand command;
            command.controlEvent = ControlEvent::CTR_ATKA;
            input.players.push_back({0, {command}});
        }
        world.tick(input);
        timeline.push_back(std::to_string(frame) + ":" + key());
    };
    tick(true);  // 施放首段
    auto& pool      = ecs.get<SkillPool>(entity);
    const int32_t first = pool.m_current;
    REQUIRE(first != kNoSkill);
    const int32_t firstStep = pool.m_skills[first].m_address.m_step;
    // 打断窗口内再按一次，续接下一段并切换当前技能。
    tick(true);
    for (int i = 0; i < 200 && ecs.get<SkillPool>(entity).m_current == first; ++i)
        tick(false);
    REQUIRE(ecs.get<SkillPool>(entity).m_current != first);
    const auto switchedAt = frame;
    tick(false);
    tick(false);
    // 钉住：当前技能切到链中下一段，段号递增且配置 id 为首段 nextSkill，管线继续推进。
    auto& switchedPool  = ecs.get<SkillPool>(entity);
    const int32_t switched = switchedPool.m_current;
    CHECK(switched != first);
    CHECK(switchedPool.m_skills[switched].m_address.m_step == firstStep + 1);
    CHECK(switchedPool.m_skills[switched].m_configId ==
          conf::Config::instance().get<conf::SkillAttackConfig>(switchedPool.m_skills[first].m_configId).nextSkill);
    std::ostringstream dump;
    for (const auto& line : timeline)
        dump << line << " ";
    std::cout << "InterruptWindow skill=" << skillId << " switched_frame=" << switchedAt << " timeline=" << dump.str() << '\n';
    conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin");
}

TEST_CASE("Room change exits previous leaf and enters plus updates once")
{
    test::ScenarioHarness::loadAssets();
    World world;
    world.init(test::ScenarioHarness::roomSetup(2071501, 31));
    sim::RoleSpawnParams player;
    player.camp       = 2;
    player.playerSlot = 0;
    const auto entity = sim::spawnPlayer(world, CharacterClass::kSwordman, player);
    REQUIRE(entity.value != 0);
    const auto* config = conf::Config::instance().find<conf::RoomConfig>(2071501);
    REQUIRE(config);
    REQUIRE(sim::room::enter(world, *config, 0, 0));
    auto& tree        = world.ecs().get<RoleTree>(entity);
    const auto& def   = behavior::definition(tree.defKey);
    REQUIRE_FALSE(tree.instance.nodes.empty());
    // 触发房间切换（传送到另一个房间）。
    world.ecs().singleton<sim::RoomState>().m_transferRoom = 2010101;
    world.tick({1, {}, {}, {}});
    // 切换后：旧叶子已退出，实例被清空，控制被清除。
    auto& afterTree = world.ecs().get<RoleTree>(entity);
    CHECK(afterTree.instance.nodes.empty());
    CHECK(afterTree.control.controlEvent == ControlEvent::CTR_NONE);
    // 切换会挂起角色，解除挂起后首帧：进入并更新一次（待机叶时间为一帧）。
    world.ecs().get<EntityGroup>(entity).suspended = false;
    world.tick({2, {}, {}, {}});
    auto& finalTree = world.ecs().get<RoleTree>(entity);
    REQUIRE_FALSE(finalTree.instance.nodes.empty());
    int32_t idleNode = -1;
    for (uint32_t i = 0; i < finalTree.instance.nodes.size(); ++i)
        if (def.topology().node(i).type == BTNodeType::Action && def.topology().node(i).leafTypeId == behavior::kIdleLeaf &&
            finalTree.instance.nodes[i].status == BTStatus::Running)
            idleNode = static_cast<int32_t>(i);
    REQUIRE(idleNode >= 0);
    const auto leafIdx = def.topology().leafIndex(static_cast<uint32_t>(idleNode));
    CHECK(std::get<behavior::ActionState>(finalTree.instance.leaves[leafIdx]).fTime == kLogicDtMs);
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
