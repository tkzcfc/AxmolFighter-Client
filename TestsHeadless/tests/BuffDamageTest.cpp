#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffDamage.h"

// 周期伤害保留符号和向下取整，不因普通伤害的非负限制而变成零。
TEST_CASE("BuffDamage signed formula has no defense critical or spread")
{
    using namespace mugen;
    using namespace mugen::sim;
    CHECK(buff::calculateBuffDamage("101"_fx, "-0.5"_fx, {}) == "-51"_fx);
    CHECK(buff::calculateBuffDamage("101"_fx, "0.5"_fx, {}) == "50"_fx);
    CHECK(buff::calculateBuffDamage("100"_fx, "-0.5"_fx, "1"_fx) == "-100"_fx);
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, RoleHitState{});
    auto& vitals = world.ecs().get<Vitals>(entity);
    vitals.hp = "100"_fx;
    vitals.maxHp = "100"_fx;
    buff::dealBuffDamage(world, entity, entity, "-25"_fx);
    CHECK(world.ecs().get<Vitals>(entity).hp == "75"_fx);
    CHECK(world.ecs().get<RoleHitState>(entity).m_dps == "25"_fx);
    const auto& events = world.ecs().singleton<PresentationEvents>().events;
    REQUIRE_FALSE(events.empty());
    REQUIRE(std::holds_alternative<HurtNumber>(events.back().payload));
    CHECK(std::get<HurtNumber>(events.back().payload).value == 25);
}
