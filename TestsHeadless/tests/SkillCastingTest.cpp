#include "SkillTestSupport.h"

using namespace skill_test;

TEST_CASE("Skill zero cooldown basic attack casts ten times without a tick")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.cd == 0 && r.mp <= 0 && r.ep <= 0 && r.crystal <= 0; });
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    for (int i = 0; i < 10; ++i)
    {
        idle(world, entity);
        REQUIRE(skill::dealButtonEvent(world, entity, ControlEvent::CTR_ATKA));
        auto& pool = world.ecs().get<SkillPool>(entity);
        skill::dealWithCastPipeBegan(world, entity, pool.m_current);
        CHECK_FALSE(skill::isCooling(pool.m_skills[pool.m_current]));
    }
}

TEST_CASE("Skill three charges retain partial cooldown and recover one per interval")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.cd > 0 && r.cd < 1000000; });
    auto row = conf::Config::instance().get<conf::SkillAttackConfig>(id);
    row.cdCount = 3;
    row.type = 0;
    ConfigOverride config({row});
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    auto& pool = world.ecs().get<SkillPool>(entity);
    const auto index = skill::getGivenSkill(pool, "A");
    for (int remaining = 2; remaining >= 0; --remaining)
    {
        REQUIRE(skill::isAllowCast(world, entity, index));
        skill::castBegan(world, entity, index);
        CHECK(pool.m_skills[index].m_releaseCount == remaining);
    }
    CHECK_FALSE(skill::isAllowCast(world, entity, index));
    const auto cd = pool.m_skills[index].m_cd;
    world.ecs().singleton<WorldClock>().dtMs = cd / "2"_fx;
    skill::update(world, entity);
    CHECK(pool.m_skills[index].m_releaseCount == 0);
    world.ecs().singleton<WorldClock>().dtMs = cd;
    skill::update(world, entity);
    CHECK(pool.m_skills[index].m_releaseCount == 1);
    CHECK(skill::isAllowCast(world, entity, index));
    const auto partial = pool.m_skills[index].m_cd;
    skill::castBegan(world, entity, index);
    CHECK(pool.m_skills[index].m_cd == partial);
    for (int remaining = 1; remaining <= 3; ++remaining)
    {
        skill::update(world, entity);
        CHECK(pool.m_skills[index].m_releaseCount == remaining);
    }
    CHECK_FALSE(pool.m_skills[index].m_timing);
}

TEST_CASE("Skill A1 A2 A3 chaining uses preset cache and pipe exit")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) {
        const auto* next = conf::Config::instance().find<conf::SkillAttackConfig>(r.nextSkill);
        return r.cd == 0 && r.cdCount <= 1 && next && next->cdCount <= 1 &&
            conf::Config::instance().find<conf::SkillAttackConfig>(next->nextSkill);
    });
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(skill::presetSkill(world, entity, "A"));
    for (int step = 1; step <= 2; ++step)
    {
        const auto current = pool.m_current;
        skill::dealWithCastPipeBegan(world, entity, current);
        REQUIRE(skill::presetSkill(world, entity, "A"));
        CHECK(pool.m_current == current);
        REQUIRE(pool.m_skills[current].m_nextSkill != kNoSkill);
        skill::dealWithCastPipeEnded(world, entity, current);
        CHECK(pool.m_skills[pool.m_current].m_address.m_step == step + 1);
    }
}

TEST_CASE("Skill high priority replaces immediately while low priority caches")
{
    loadConfig();
    const auto low = findSkill([](const auto& r) { return r.sorder >= 0 && r.sorder < 50; });
    const auto order = conf::Config::instance().get<conf::SkillAttackConfig>(low).sorder;
    const auto high = findSkill([&](const auto& r) { return r.sorder > order; });
    World world;
    const auto entity = actor(world);
    slots(world, entity, {low, high});
    auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(skill::presetSkill(world, entity, "A"));
    pool.m_allowInterrupt = true;
    REQUIRE(skill::presetSkill(world, entity, "B"));
    CHECK(pool.m_skills[pool.m_current].m_configId == high);
    const auto current = pool.m_current;
    pool.m_allowInterrupt = true;
    REQUIRE(skill::presetSkill(world, entity, "A"));
    CHECK(pool.m_current == current);
    CHECK(pool.m_skills[pool.m_skills[current].m_nextSkill].m_configId == low);
}

TEST_CASE("Skill silence stun and resource refusal emit body shake hint")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.mp > 0 && r.ep > 0 && r.crystal <= 0; });
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    const auto index = skill::getGivenSkill(world.ecs().get<SkillPool>(entity), "A");
    SUBCASE("silence") { world.ecs().get<sim::ExtraState>(entity).add(mugen::ExtraState::Silence); }
    SUBCASE("stun") { world.ecs().get<sim::ExtraState>(entity).add(mugen::ExtraState::Stun); }
    SUBCASE("MP") { world.ecs().get<Vitals>(entity).mp = {}; }
    SUBCASE("EP") { world.ecs().get<Vitals>(entity).ep = {}; }
    CHECK_FALSE(skill::isAllowCast(world, entity, index));
    const auto& events = world.ecs().singleton<PresentationEvents>().events;
    REQUIRE(events.size() == 1);
    REQUIRE(std::holds_alternative<Tips>(events[0].payload));
    CHECK(std::get<Tips>(events[0].payload).text == "body shake hint");
}
