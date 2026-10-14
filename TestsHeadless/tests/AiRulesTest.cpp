#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/core/math/Random.h"
#include "mugen/sim/ai/AiAgent.h"
#include "mugen/sim/ai/AiRules.h"
#include <array>

using namespace mugen;
using namespace mugen::sim;
using namespace mugen::sim::ai;

namespace
{
// 读出随机数四个状态字，用于比较是否消耗了随机流。
struct RngWords
{
    std::array<uint64_t, 4> m_words{};
    uint8_t m_count = 0;
    void field(const char*, uint64_t value) { m_words[m_count++] = value; }
    bool operator==(const RngWords&) const = default;
};
RngWords rngWords(const Random& rng)
{
    RngWords words;
    Random::mgReflect(words, rng);
    return words;
}
// 构造使指定行成为怪物表第一条命中的快照。
AiFacts factsFor(int row)
{
    AiFacts facts;
    facts.m_hpPercent = 100;
    switch (row)
    {
    case 0: facts.m_provoked = true; break;
    case 1: facts.m_lastAction = AiAction::Patrol; break;
    case 2: break;
    case 3:
        facts.m_alerted    = true;
        facts.m_hasTarget  = true;
        facts.m_skillReady = true;
        break;
    case 4:
        facts.m_alerted            = true;
        facts.m_hasTarget          = true;
        facts.m_targetOutsideChase = true;
        break;
    case 5:
        facts.m_alerted   = true;
        facts.m_hasTarget = true;
        break;
    case 6: facts.m_alerted = true; break;
    default: break;
    }
    return facts;
}
AiRule ruleOf(std::initializer_list<AiTest> tests, AiAction action)
{
    AiRule rule;
    rule.m_action    = action;
    rule.m_testCount = static_cast<uint8_t>(tests.size());
    uint8_t index    = 0;
    for (const auto& test : tests) rule.m_tests[index++] = test;
    return rule;
}
}  // namespace

TEST_CASE("Ai rules select the first matching monster row")
{
    Random rng(1);
    const auto before = rngWords(rng);
    for (int row = 0; row < 7; ++row)
        CHECK(pickRule(factsFor(row), kMonsterRules, {}, rng) == static_cast<int8_t>(row));
    CHECK(rngWords(rng) == before);
    CHECK(kMonsterRules[0].m_setsAlert);
    CHECK(kMonsterRules[0].m_action == AiAction::CastWakeSkill);
    CHECK(kMonsterRules[1].m_action == AiAction::CastIdleSkill);
}

TEST_CASE("Ai rules skip a cooling row and tick its cooldown")
{
    Random rng(1);
    std::array<Fixed, 7> cooldown{};
    cooldown[2] = Fixed::fromInt(100);
    CHECK(pickRule(factsFor(2), kMonsterRules, cooldown, rng) == 6);
    tickRuleCooldowns(cooldown, Fixed::fromInt(40));
    CHECK(cooldown[2] == Fixed::fromInt(60));
    CHECK(pickRule(factsFor(2), kMonsterRules, cooldown, rng) == 6);
    tickRuleCooldowns(cooldown, Fixed::fromInt(60));
    CHECK(cooldown[2] == Fixed{});
    CHECK(pickRule(factsFor(2), kMonsterRules, cooldown, rng) == 2);
}

TEST_CASE("Ai rules preempt a running move only with an earlier row")
{
    Random rng(1);
    auto wake = factsFor(0);
    wake.m_movingRule = 2;
    CHECK(pickRule(wake, kMonsterRules, {}, rng) == 0);

    auto chase = factsFor(4);
    chase.m_movingRule = 4;
    CHECK(pickRule(chase, kMonsterRules, {}, rng) == -1);

    auto attacking = factsFor(0);
    attacking.m_inAttack = true;
    CHECK(pickRule(attacking, kMonsterRules, {}, rng) == -1);
}

TEST_CASE("Ai rules draw ChancePercent only after earlier tests pass")
{
    const auto rules = std::array{
        ruleOf({{AiCondition::Alerted}, {AiCondition::ChancePercent, false, 100}}, AiAction::CastBestSkill),
        ruleOf({{AiCondition::Always}}, AiAction::Idle),
    };
    AiFacts blocked;
    Random untouched(3);
    CHECK(pickRule(blocked, rules, {}, untouched) == 1);
    CHECK(rngWords(untouched) == rngWords(Random(3)));

    AiFacts open;
    open.m_alerted = true;
    Random drawn(3);
    CHECK(pickRule(open, rules, {}, drawn) == 0);
    Random once(3);
    once.randomN(1, 100);
    CHECK(rngWords(drawn) == rngWords(once));

    const auto miss = std::array{
        ruleOf({{AiCondition::Alerted}, {AiCondition::ChancePercent, false, 0}}, AiAction::CastBestSkill),
        ruleOf({{AiCondition::Always}}, AiAction::Idle),
    };
    Random failed(9);
    CHECK(pickRule(open, miss, {}, failed) == 1);
    Random onceFail(9);
    onceFail.randomN(1, 100);
    CHECK(rngWords(failed) == rngWords(onceFail));

    std::array<Fixed, 1> cooling{Fixed::fromInt(1)};
    Random cooled(3);
    CHECK(pickRule(open, rules, cooling, cooled) == 1);
    CHECK(rngWords(cooled) == rngWords(Random(3)));
}

TEST_CASE("Ai rules gather reads an offline scenario monster")
{
    auto world = test::ScenarioHarness::roomWorld(2010101);
    Entity monster{};
    world.ecs().each<RoleIdentity, AiAgent>([&](Entity entity, RoleIdentity&, AiAgent&) {
        if (monster.value == 0) monster = entity;
    });
    REQUIRE(monster.value != 0);
    const auto facts  = AiDriver(world, monster).gather();
    const auto& state = world.ecs().get<RoleStatus>(monster);
    const auto& agent = world.ecs().get<AiAgent>(monster);
    const auto& pool  = world.ecs().get<SkillPool>(monster);
    const auto& vitals = world.ecs().get<Vitals>(monster);
    CHECK(facts.m_alerted == (agent.m_tree.m_alert == AlertStatus::Alert));
    CHECK(facts.m_inAttack == state.isAttack());
    CHECK(facts.m_hasTarget == (pool.m_target != monster && world.ecs().tryGet<Transform>(pool.m_target) != nullptr));
    REQUIRE(vitals.maxHp > Fixed{});
    CHECK(facts.m_hpPercent == static_cast<int32_t>((vitals.hp * Fixed::fromInt(100) / vitals.maxHp).toInt()));
    CHECK_FALSE(facts.m_alerted);
    CHECK_FALSE(facts.m_inAttack);
}
