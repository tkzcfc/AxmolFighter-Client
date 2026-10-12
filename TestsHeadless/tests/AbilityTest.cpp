#include "SkillTestSupport.h"
#include "mugen/sim/ability/AbilityPool.h"

namespace
{
// 查找可无条件触发指定事件的生产能力配置。
const mugen::conf::SpecialAbilityConfig& abilityFor(mugen::sim::ability::Event event, int32_t excludedId = -1)
{
    using namespace mugen;
    const auto& rows = mugen::conf::Config::instance().table<mugen::conf::SpecialAbilityConfig>();
    const auto row = std::find_if(rows.begin(), rows.end(), [event, excludedId](const auto& value) {
        return value.id != excludedId && value.condition == -1 && value.bindSkillType == 0 && value.probability >= "100"_fx &&
            std::find(value.event.begin(), value.event.end(), static_cast<int32_t>(event)) != value.event.end();
    });
    REQUIRE(row != rows.end());
    return *row;
}
}

// 三个不同事件均通过生产配置的事件入口激活能力。
TEST_CASE("Ability pool dispatches cast and hit events")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const Entity entity = skill_test::actor(world);
    const auto& before = abilityFor(ability::Event::BeforeCastSkill);
    REQUIRE(ability::add(world, entity, before.id));
    ability::trigger(world, entity, ability::Event::BeforeCastSkill);
    CHECK(world.ecs().get<AbilityPool>(entity).m_instances.front().m_active);
    ability::clear(world, entity);
    const auto& after = abilityFor(ability::Event::AfterCastSkill);
    REQUIRE(ability::add(world, entity, after.id));
    ability::trigger(world, entity, ability::Event::AfterCastSkill);
    CHECK(world.ecs().get<AbilityPool>(entity).m_instances.front().m_active);
    ability::clear(world, entity);
    const auto& hit = abilityFor(ability::Event::AfterHit);
    REQUIRE(ability::add(world, entity, hit.id));
    ability::trigger(world, entity, ability::Event::AfterHit);
    CHECK(world.ecs().get<AbilityPool>(entity).m_instances.front().m_active);
}

// 未满足暴击条件时能力不会触发，满足后才进入激活状态。
TEST_CASE("Ability condition blocks non-critical event")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    const Entity entity = skill_test::actor(world);
    conf::SpecialAbilityConfig config;
    config.condition = 1;
    AbilityContext context;
    CHECK_FALSE(ability::checkConditions(world, entity, config, context));
    context.m_critical = true;
    CHECK(ability::checkConditions(world, entity, config, context));
}
