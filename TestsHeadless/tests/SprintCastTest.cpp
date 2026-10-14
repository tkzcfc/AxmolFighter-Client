#include "SkillTestSupport.h"

using namespace skill_test;

namespace
{
// 普通攻击与冲刺的配置对。
struct SprintLoad
{
    int32_t m_attack;
    int32_t m_sprint;
};

// dealButtonEvent 之后当前技能应落在的槽位与配置。
struct ExpectedSlot
{
    std::string_view m_slot;
    int32_t m_configId;
};

// 组 1 第 1 槽为 A，特殊组在 3 个槽位时第 2 槽为 Y。
void loadAttackAndSprint(World& world, Entity entity, SprintLoad ids)
{
    skill::SkillLoadout data(5);
    data[0].push_back({{{ids.m_attack, -1}}});
    data[4].resize(3);
    data[4][1].push_back({{ids.m_sprint, -1}});
    skill::SkillCaster(world, entity).loadSkill(data);
    world.ecs().get<SkillPool>(entity).m_crystals = 1000000;
}

int32_t chainHead()
{
    return findSkill([](const auto& row) {
        const auto* next = conf::Config::instance().find<conf::SkillAttackConfig>(row.nextSkill);
        return row.cd == 0 && row.mp <= 0 && row.ep <= 0 && row.crystal <= 0 && next && next->id != row.id &&
               next->cd == 0 && next->mp <= 0 && next->ep <= 0 && next->crystal <= 0;
    });
}

int32_t otherCheap(int32_t skip)
{
    return findSkill([skip](const auto& row) {
        return row.id != skip && row.cd == 0 && row.mp <= 0 && row.ep <= 0 && row.crystal <= 0;
    });
}

void expectCurrent(World& world, Entity entity, ExpectedSlot expected)
{
    const auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(pool.m_current != kNoSkill);
    const auto& skill = pool.m_skills[pool.m_current];
    CHECK(skill::baseSlot(pool, skill) == expected.m_slot);
    CHECK(skill.m_configId == expected.m_configId);
}
}  // namespace

TEST_CASE("Sprint attack A during run selects Y through dealButtonEvent")
{
    loadConfig();
    const auto sprint = chainHead();
    const auto attack = otherCheap(sprint);
    World world;
    const auto entity = actor(world);
    loadAttackAndSprint(world, entity, {attack, sprint});
    auto& pool                                        = world.ecs().get<SkillPool>(entity);
    pool.m_canSprint                                  = true;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Run;
    REQUIRE(world.ecs().get<RoleStatus>(entity).isRun());

    REQUIRE(skill::SkillCaster(world, entity).dealButtonEvent(ControlEvent::CTR_ATKA));

    expectCurrent(world, entity, {"Y", sprint});
}

TEST_CASE("Sprint attack A during run keeps A when sprint is disabled")
{
    loadConfig();
    const auto sprint = chainHead();
    const auto attack = otherCheap(sprint);
    World world;
    const auto entity = actor(world);
    loadAttackAndSprint(world, entity, {attack, sprint});
    auto& pool                                        = world.ecs().get<SkillPool>(entity);
    pool.m_canSprint                                  = false;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Run;

    REQUIRE(skill::SkillCaster(world, entity).dealButtonEvent(ControlEvent::CTR_ATKA));

    expectCurrent(world, entity, {"A", attack});
}

TEST_CASE("Sprint attack A during a non-final Y skill selects Y")
{
    loadConfig();
    const auto sprint = chainHead();
    const auto attack = otherCheap(sprint);
    World world;
    const auto entity = actor(world);
    loadAttackAndSprint(world, entity, {attack, sprint});
    auto& pool                                        = world.ecs().get<SkillPool>(entity);
    pool.m_canSprint                                  = true;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Idle;
    const auto y                                      = skill::getGivenSkill(pool, "Y");
    REQUIRE(y != kNoSkill);
    REQUIRE_FALSE(pool.m_skills[y].m_finally);
    skill::setSkillBase(pool, y);
    REQUIRE_FALSE(world.ecs().get<RoleStatus>(entity).isRun());

    REQUIRE(skill::SkillCaster(world, entity).dealButtonEvent(ControlEvent::CTR_ATKA));

    expectCurrent(world, entity, {"Y", sprint});
}

TEST_CASE("Sprint attack A while idle keeps A")
{
    loadConfig();
    const auto sprint = chainHead();
    const auto attack = otherCheap(sprint);
    World world;
    const auto entity = actor(world);
    loadAttackAndSprint(world, entity, {attack, sprint});
    auto& pool                                        = world.ecs().get<SkillPool>(entity);
    pool.m_canSprint                                  = true;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Idle;
    REQUIRE(pool.m_current == kNoSkill);

    REQUIRE(skill::SkillCaster(world, entity).dealButtonEvent(ControlEvent::CTR_ATKA));

    expectCurrent(world, entity, {"A", attack});
}
