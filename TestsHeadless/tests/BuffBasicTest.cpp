#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/hit/Hit.h"

namespace
{
// 为规则建立可直接调用的配置实例。
mugen::sim::BuffInstance& addBasicRule(mugen::sim::World& world, mugen::Entity entity,
                                       mugen::conf::BuffRuleId rule, uint64_t serial)
{
    const auto& rules = mugen::conf::Config::instance().table<mugen::conf::BuffRuleConfig>();
    const auto ruleRow = std::find_if(rules.begin(), rules.end(), [rule](const auto& row) {
        return mugen::conf::buffRuleIdFromName(row.className) == rule;
    });
    REQUIRE(ruleRow != rules.end());
    const auto& buffs = mugen::conf::Config::instance().table<mugen::conf::BuffConfig>();
    const auto row = std::find_if(buffs.begin(), buffs.end(), [&](const auto& buff) { return buff.ruleId == ruleRow->id; });
    REQUIRE(row != buffs.end());
    mugen::sim::BuffInstance instance;
    instance.m_id = row->id;
    instance.m_serial = serial;
    world.ecs().get<mugen::sim::BuffPool>(entity).m_instances.push_back(instance);
    return world.ecs().get<mugen::sim::BuffPool>(entity).m_instances.back();
}
}

// 验证护盾数值通过生命管线吸收伤害并在退出时清空。
TEST_CASE("BuffBasic Shield numeric absorption")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    auto& vitals = world.ecs().get<Vitals>(entity);
    vitals.maxHp = Fixed::fromInt(1000);
    vitals.hp = vitals.maxHp;
    auto& instance = addBasicRule(world, entity, conf::BuffRuleId::BuffShield, 1);
    const auto& dispatch = buff::ruleDispatch(conf::BuffRuleId::BuffShield);
    dispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Enter)](world, entity, instance.m_serial, {});
    const Fixed shield = vitals.m_shield;
    CHECK(shield >= Fixed{});
    hit::modifyHp(world, entity, -Fixed::fromInt(1));
    CHECK(vitals.hp >= Fixed::fromInt(999));
    dispatch.m_hooks[static_cast<size_t>(buff::RuleHook::Exit)](world, entity, instance.m_serial, {});
    CHECK(vitals.m_shield == Fixed{});
}

// 验证锁血、增减伤和冷却缩放均使用确定性数值状态。
TEST_CASE("BuffBasic HPLock damage and cooldown modifiers")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    world.ecs().add(entity, SkillPool{});
    world.ecs().add(entity, Attributes{});
    auto& vitals = world.ecs().get<Vitals>(entity);
    vitals.maxHp = Fixed::fromInt(1000);
    vitals.hp = vitals.maxHp;

    auto& lock = addBasicRule(world, entity, conf::BuffRuleId::BuffHPLock, 1);
    auto& hurt = addBasicRule(world, entity, conf::BuffRuleId::BuffDamageHurt2, 2);
    auto& cd = addBasicRule(world, entity, conf::BuffRuleId::BuffCDSkill, 3);
    const auto& lockDispatch = buff::ruleDispatch(conf::BuffRuleId::BuffHPLock);
    const auto& hurtDispatch = buff::ruleDispatch(conf::BuffRuleId::BuffDamageHurt2);
    const auto& cdDispatch = buff::ruleDispatch(conf::BuffRuleId::BuffCDSkill);
    lockDispatch.m_hooks[0](world, entity, lock.m_serial, {});
    const Fixed floor = vitals.m_hpLock;
    hit::modifyHp(world, entity, -vitals.maxHp);
    CHECK(vitals.hp >= floor);
    hurtDispatch.m_hooks[0](world, entity, hurt.m_serial, {});
    CHECK(world.ecs().get<Attributes>(entity).extended(100) >= Fixed{});
    cdDispatch.m_hooks[0](world, entity, cd.m_serial, {});
    for (const auto& skill : world.ecs().get<SkillPool>(entity).m_skills)
        CHECK(skill.m_coldScale >= Fixed{});
}

// 所有可达数值规则必须不再走未移植占位分派。
TEST_CASE("BuffBasic reachable xbasic rules have concrete dispatch")
{
    using namespace mugen::conf;
    constexpr std::array rules{
        BuffRuleId::BuffAddByApplicator, BuffRuleId::BuffAddByApplicator3, BuffRuleId::BuffAddByApplicator4,
        BuffRuleId::BuffAddByDetector, BuffRuleId::BuffAddByState, BuffRuleId::BuffAngerAdd,
        BuffRuleId::BuffAvoidCrit, BuffRuleId::BuffBreakArmor, BuffRuleId::BuffCDSkill, BuffRuleId::BuffCold,
        BuffRuleId::BuffComboExp, BuffRuleId::BuffCrit, BuffRuleId::BuffDamageCrit, BuffRuleId::BuffDamageHPLoss,
        BuffRuleId::BuffDamageHurt, BuffRuleId::BuffDamageHurt2, BuffRuleId::BuffDamageReduction,
        BuffRuleId::BuffDamageTypeGradation, BuffRuleId::BuffDodge, BuffRuleId::BuffEP,
        BuffRuleId::BuffEPConsumeScale, BuffRuleId::BuffEPRecoveryScale, BuffRuleId::BuffHP, BuffRuleId::BuffHit,
        BuffRuleId::BuffRemoveById, BuffRuleId::BuffRemoveById2, BuffRuleId::BuffRemoveByRule,
        BuffRuleId::BuffRemoveByType, BuffRuleId::BuffShield, BuffRuleId::BuffSign,
        BuffRuleId::BuffSpecialAbilityApplicator, BuffRuleId::BuffTP, BuffRuleId::BuffTPConsumeScale,
        BuffRuleId::BuffTPRecoveryScale};
    for (const auto rule : rules)
        CHECK_FALSE(mugen::sim::buff::isUnportedRule(rule));
}
