#include "AiRules.h"
#include "mugen/core/math/Random.h"
#include <limits>

namespace mugen::sim::ai
{
namespace
{
// ChancePercent 只能出现在本行最后，避免前面的判断失败前就消耗随机数。
void assertChanceLast(const AiRule& rule)
{
    MG_ASSERT(rule.m_testCount <= rule.m_tests.size() && "AI rule has too many tests");
    for (uint8_t index = 0; index < rule.m_testCount; ++index)
        if (rule.m_tests[index].m_kind == AiCondition::ChancePercent)
            MG_ASSERT(index + 1 == rule.m_testCount && "ChancePercent must be the last test");
}
// 计算单项判断；只有 ChancePercent 会推进传入的随机数。
bool passes(const AiTest& test, const AiFacts& facts, Random& rng)
{
    bool pass = false;
    switch (test.m_kind)
    {
    case AiCondition::Always: pass = true; break;
    case AiCondition::Alerted: pass = facts.m_alerted; break;
    case AiCondition::Provoked: pass = facts.m_provoked; break;
    case AiCondition::HasTarget: pass = facts.m_hasTarget; break;
    case AiCondition::TargetInSight: pass = facts.m_targetInSight; break;
    case AiCondition::TargetOutsideChase: pass = facts.m_targetOutsideChase; break;
    case AiCondition::SkillReady: pass = facts.m_skillReady; break;
    case AiCondition::LastActionIs: pass = static_cast<int32_t>(facts.m_lastAction) == test.m_value; break;
    case AiCondition::HpBelowPercent: pass = facts.m_hpPercent < test.m_value; break;
    case AiCondition::ChancePercent: pass = rng.randomN(1, 100) <= test.m_value; break;
    }
    return test.m_negate ? !pass : pass;
}
// 本行全部有效判断都成立才命中。
bool matches(const AiRule& rule, const AiFacts& facts, Random& rng)
{
    assertChanceLast(rule);
    for (uint8_t index = 0; index < rule.m_testCount; ++index)
        if (!passes(rule.m_tests[index], facts, rng)) return false;
    return true;
}
}  // namespace

int8_t pickRule(const AiFacts& facts, std::span<const AiRule> rules, std::span<const Fixed> ruleCooldownMs, Random& rng)
{
    if (facts.m_inAttack) return -1;
    MG_ASSERT(rules.size() <= static_cast<size_t>(std::numeric_limits<int8_t>::max()) && "Rule index must fit in int8_t");
    for (size_t index = 0; index < rules.size(); ++index)
    {
        if (facts.m_movingRule >= 0 && static_cast<int8_t>(index) >= facts.m_movingRule) return -1;
        if (index < ruleCooldownMs.size() && ruleCooldownMs[index] > Fixed{}) continue;
        if (matches(rules[index], facts, rng)) return static_cast<int8_t>(index);
    }
    return -1;
}

void tickRuleCooldowns(std::span<Fixed> ruleCooldownMs, Fixed dtMs)
{
    for (Fixed& remaining : ruleCooldownMs)
        if (remaining > Fixed{}) remaining = remaining > dtMs ? remaining - dtMs : Fixed{};
}
}  // namespace mugen::sim::ai
