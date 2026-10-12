#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffRules.h"

// 通过生产分派表验证状态引用和所有规则的非空钩子。
TEST_CASE("BuffRules dispatch owns state references")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    uint64_t serial = 1;
    for (const auto rule : {conf::BuffRuleId::BuffSuperArmor, conf::BuffRuleId::BuffInvincible})
    {
        const auto& rules = conf::Config::instance().table<conf::BuffRuleConfig>();
        const auto ruleRow = std::find_if(rules.begin(), rules.end(), [rule](const auto& row) {
            return conf::buffRuleIdFromName(row.className) == rule;
        });
        REQUIRE(ruleRow != rules.end());
        const auto& buffs = conf::Config::instance().table<conf::BuffConfig>();
        const auto row = std::find_if(buffs.begin(), buffs.end(), [&](const auto& buff) {
            return buff.ruleId == ruleRow->id;
        });
        REQUIRE(row != buffs.end());
        BuffInstance instance;
        instance.m_id = row->id;
        instance.m_serial = serial++;
        world.ecs().get<BuffPool>(entity).m_instances.push_back(instance);
        const auto state = rule == conf::BuffRuleId::BuffSuperArmor ?
            mugen::ExtraState::SuperArmor : mugen::ExtraState::Invincible;
        const auto& dispatch = buff::ruleDispatch(rule);
        dispatch.m_hooks[0](world, entity, instance.m_serial, {});
        dispatch.m_hooks[0](world, entity, instance.m_serial, {});
        CHECK(world.ecs().get<sim::ExtraState>(entity).active(state));
        CHECK(world.ecs().get<BuffPool>(entity).m_ruleCounters.at(row->ruleId) == 2);
        dispatch.m_hooks[1](world, entity, instance.m_serial, {});
        CHECK(world.ecs().get<sim::ExtraState>(entity).active(state));
        dispatch.m_hooks[1](world, entity, instance.m_serial, {});
        CHECK_FALSE(world.ecs().get<sim::ExtraState>(entity).active(state));
    }
    for (size_t index = 0; index < conf::kBuffRuleNames.size(); ++index)
        for (auto hook : buff::ruleDispatch(static_cast<conf::BuffRuleId>(index)).m_hooks)
            REQUIRE(hook != nullptr);
}
