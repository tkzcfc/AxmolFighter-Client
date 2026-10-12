#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/motion/Displacement.h"

namespace
{
// 为指定规则建立一个可直接调用生命周期钩子的池内实例。
mugen::sim::BuffInstance& addRuleInstance(mugen::sim::World& world,
                                          mugen::Entity entity,
                                          mugen::conf::BuffRuleId rule,
                                          uint64_t serial)
{
    const auto& rules  = mugen::conf::Config::instance().table<mugen::conf::BuffRuleConfig>();
    const auto ruleRow = std::find_if(rules.begin(), rules.end(), [rule](const auto& row) {
        return mugen::conf::buffRuleIdFromName(row.className) == rule;
    });
    REQUIRE(ruleRow != rules.end());
    const auto& buffs = mugen::conf::Config::instance().table<mugen::conf::BuffConfig>();
    const auto row =
        std::find_if(buffs.begin(), buffs.end(), [&](const auto& buff) { return buff.ruleId == ruleRow->id; });
    REQUIRE(row != buffs.end());
    mugen::sim::BuffInstance instance;
    instance.m_id     = row->id;
    instance.m_serial = serial;
    world.ecs().get<mugen::sim::BuffPool>(entity).m_instances.push_back(instance);
    return world.ecs().get<mugen::sim::BuffPool>(entity).m_instances.back();
}
}  // namespace

// 验证状态型规则共享引用计数、速度恢复和速度修正生命周期。
TEST_CASE("BuffStatus control and movement rule families preserve state")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    world.ecs().add(entity, Displacement{});
    world.ecs().add(entity, Attributes{});
    world.ecs().add(entity, RoleTimers{});

    auto& frozen               = addRuleInstance(world, entity, conf::BuffRuleId::BuffFrozen, 1);
    const auto& frozenDispatch = buff::ruleDispatch(conf::BuffRuleId::BuffFrozen);
    frozenDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Enter)](world, entity, frozen.m_serial, {});
    CHECK(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Frozen));
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.x == Fixed{});
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.z == Fixed{});
    frozenDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Exit)](world, entity, frozen.m_serial, {});
    CHECK_FALSE(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Frozen));
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.x == "1"_fx);
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.z == "1"_fx);

    auto& speed               = addRuleInstance(world, entity, conf::BuffRuleId::BuffSpeed, 2);
    const auto& speedDispatch = buff::ruleDispatch(conf::BuffRuleId::BuffSpeed);
    const auto baseX          = world.ecs().get<Displacement>(entity).velocityScale.x;
    const auto baseZ          = world.ecs().get<Displacement>(entity).velocityScale.z;
    speedDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Enter)](world, entity, speed.m_serial, {});
    const bool speedChanged = world.ecs().get<Displacement>(entity).velocityScale.x != baseX ||
                              world.ecs().get<Displacement>(entity).velocityScale.z != baseZ;
    CHECK(speedChanged);
    speedDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Begin)](world, entity, speed.m_serial, {});
    const bool beginDidNotStack = world.ecs().get<Displacement>(entity).velocityScale.x != baseX ||
                                  world.ecs().get<Displacement>(entity).velocityScale.z != baseZ;
    CHECK(beginDidNotStack);
    speedDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Exit)](world, entity, speed.m_serial, {});
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.x == baseX);
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.z == baseZ);
}

// 验证嘲讽查询和白帧事件均停留在模拟边界。
TEST_CASE("BuffStatus Sneer query and WhiteFrame presentation event")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    const auto source = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    world.ecs().add(entity, RoleTimers{});
    auto& sneer                  = addRuleInstance(world, entity, conf::BuffRuleId::BuffSneer, 1);
    sneer.m_context.m_applicator = source;
    const auto& sneerDispatch    = buff::ruleDispatch(conf::BuffRuleId::BuffSneer);
    sneerDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Enter)](world, entity, sneer.m_serial, {});
    CHECK(buff::sneerTarget(world, entity) == source);
    sneerDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Exit)](world, entity, sneer.m_serial, {});
    CHECK(buff::sneerTarget(world, entity).value == 0);

    auto& whiteFrame               = addRuleInstance(world, entity, conf::BuffRuleId::BuffWhiteFrame, 2);
    const auto& whiteFrameDispatch = buff::ruleDispatch(conf::BuffRuleId::BuffWhiteFrame);
    whiteFrameDispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Enter)](world, entity, whiteFrame.m_serial, {});
    const auto& event = world.ecs().singleton<PresentationEvents>().events.back();
    REQUIRE(std::holds_alternative<WhiteFrame>(event.payload));
    CHECK(std::get<WhiteFrame>(event.payload).m_enabled);
}

// 可达状态规则必须不再指向未移植占位分派。
TEST_CASE("BuffStatus reachable xstatus rules have concrete dispatch")
{
    using namespace mugen;
    using namespace mugen::sim;
    constexpr std::array rules{
        conf::BuffRuleId::BuffAdjudication, conf::BuffRuleId::BuffBleeding,   conf::BuffRuleId::BuffBurn,
        conf::BuffRuleId::BuffChill,        conf::BuffRuleId::BuffCrazy,      conf::BuffRuleId::BuffCutSakura,
        conf::BuffRuleId::BuffDark,         conf::BuffRuleId::BuffDarkMark,   conf::BuffRuleId::BuffDarkMark2,
        conf::BuffRuleId::BuffDeathMark,    conf::BuffRuleId::BuffDurance,    conf::BuffRuleId::BuffElectricEye,
        conf::BuffRuleId::BuffElectron,     conf::BuffRuleId::BuffElectron2,  conf::BuffRuleId::BuffFall,
        conf::BuffRuleId::BuffFrozen,       conf::BuffRuleId::BuffGhostWalk,  conf::BuffRuleId::BuffHPLock,
        conf::BuffRuleId::BuffImmuneDebuff, conf::BuffRuleId::BuffInvincible, conf::BuffRuleId::BuffLightMark,
        conf::BuffRuleId::BuffLoopCheck,    conf::BuffRuleId::BuffLoopCheck2, conf::BuffRuleId::BuffMight,
        conf::BuffRuleId::BuffPalsy,        conf::BuffRuleId::BuffPoison,     conf::BuffRuleId::BuffPurplePoison,
        conf::BuffRuleId::BuffRepent,       conf::BuffRuleId::BuffReverse,    conf::BuffRuleId::BuffSeparate,
        conf::BuffRuleId::BuffSkyProtect,   conf::BuffRuleId::BuffSneer,      conf::BuffRuleId::BuffSpeed,
        conf::BuffRuleId::BuffSpeedStep,    conf::BuffRuleId::BuffStarShield, conf::BuffRuleId::BuffStealth,
        conf::BuffRuleId::BuffStoneSkin,    conf::BuffRuleId::BuffSuperArmor, conf::BuffRuleId::BuffSuperBlind,
        conf::BuffRuleId::BuffThunder1,     conf::BuffRuleId::BuffThunder2,   conf::BuffRuleId::BuffUnstable,
        conf::BuffRuleId::BuffWhiteFrame};
    for (const auto rule : rules)
        CHECK_FALSE(buff::isUnportedRule(rule));
}
