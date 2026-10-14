#include "AiRules.h"
#include "AiAgent.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/Random.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/role/Role.h"
#include <limits>

namespace mugen::sim::ai
{
namespace
{
// 与索敌、追击相同的定点绝对值。
Fixed absolute(Fixed value) { return value < Fixed{} ? -value : value; }
// 缺失的区间端点按零处理。
Fixed bound(const std::vector<int32_t>& range, size_t index)
{
    return index < range.size() ? Fixed::fromInt(range[index]) : Fixed{};
}
// 有符号偏移是否落在闭区间内。
bool contains(const std::vector<int32_t>& range, Fixed value)
{
    return range.size() >= 2 && bound(range, 0) <= value && value <= bound(range, 1);
}
// 嘲讽来源仍然是活着的角色。
bool living(World& world, Entity entity)
{
    const auto* state = world.ecs().tryGet<RoleStatus>(entity);
    return state && !state->isDeath() && !Role(world, entity).isDead();
}
// 受击、仇恨记录或有效嘲讽都算激怒。
bool provoked(World& world, Entity entity)
{
    if (world.ecs().get<RoleStatus>(entity).isTobeHit()) return true;
    if (!world.ecs().get<AiAgent>(entity).m_threat.empty()) return true;
    return living(world, buff::sneerTarget(world, entity));
}
// 索敌使用带符号的视野区间，先看已锁定目标再看候选者。
bool inSight(World& world, Entity entity, const conf::AiConfig& row)
{
    const auto origin = world.ecs().get<Transform>(entity).position;
    const auto locked = world.ecs().get<SkillPool>(entity).m_target;
    if (const auto* other = world.ecs().tryGet<Transform>(locked))
        if (locked != entity && contains(row.targetScopeX, origin.x - other->position.x) &&
            contains(row.targetScopeZ, origin.z - other->position.z))
            return true;
    for (const auto other : candidates(world, entity))
    {
        const auto pos = world.ecs().get<Transform>(other).position;
        if (contains(row.targetScopeX, origin.x - pos.x) && contains(row.targetScopeZ, origin.z - pos.z)) return true;
    }
    return false;
}
// 追击范围全零、重合或落在范围内时都不算范围外。
bool outsideChase(World& world, Entity entity, Entity target, const conf::AiConfig& row)
{
    const auto* other = world.ecs().tryGet<Transform>(target);
    if (!other) return false;
    if (bound(row.chaseScopeX, 0) == Fixed{} && bound(row.chaseScopeX, 1) == Fixed{} &&
        bound(row.chaseScopeZ, 0) == Fixed{} && bound(row.chaseScopeZ, 1) == Fixed{})
        return false;
    const auto self = world.ecs().get<Transform>(entity).position;
    const Fixed x = self.x - other->position.x, z = self.z - other->position.z;
    if (x == Fixed{} && z == Fixed{}) return false;
    if (x != Fixed{} && z != Fixed{} && contains(row.chaseScopeX, absolute(x)) && contains(row.chaseScopeZ, absolute(z)))
        return false;
    return true;
}
// 只读施法间隔：尚未写入池子时用配置里的初始冷却判断。
bool skillReady(const SkillPool& pool, const conf::AiConfig* row)
{
    if (!pool.m_aiSkillCastInterval.empty())
    {
        for (const Fixed value : pool.m_aiSkillCastInterval)
            if (value <= Fixed{}) return true;
        return false;
    }
    if (!row) return false;
    for (const auto& values : row->skillPriorityLevelCd)
        if ((values.empty() ? row->skillInterval : values.front()) <= 0) return true;
    return false;
}
// 巡逻、追击和游走对应条件表中的移动行。
int8_t ruleIndex(AiAction action)
{
    for (size_t index = 0; index < kMonsterRules.size(); ++index)
        if (kMonsterRules[index].m_action == action) return static_cast<int8_t>(index);
    return -1;
}
// 当前移动状态映射到正在跑的规则；没有移动时为 -1。
int8_t movingRule(const RoleStatus& state)
{
    if (state.isPatrol()) return ruleIndex(AiAction::Patrol);
    if (state.isChase()) return ruleIndex(AiAction::Chase);
    if (state.isAlert()) return ruleIndex(AiAction::Wander);
    return -1;
}
// 生命上限不为正时百分比为零。
int32_t hpPercent(const Vitals& vitals)
{
    if (vitals.maxHp <= Fixed{}) return 0;
    return static_cast<int32_t>((vitals.hp * Fixed::fromInt(100) / vitals.maxHp).toInt());
}
// 攻击状态视为已经开始的动作；其余非移动状态记为待机。
AiAction finishedAction(const RoleStatus& state)
{
    return state.isAttack() ? AiAction::CastBestSkill : AiAction::Idle;
}
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

AiFacts gather(World& world, Entity entity)
{
    auto& ecs            = world.ecs();
    const auto& state    = ecs.get<RoleStatus>(entity);
    const auto& vitals   = ecs.get<Vitals>(entity);
    const auto& pool     = ecs.get<SkillPool>(entity);
    const auto* row      = conf::Config::instance().find<conf::AiConfig>(ecs.get<RoleIdentity>(entity).aiId);
    AiFacts facts;
    facts.m_alerted            = ecs.get<AiAgent>(entity).m_tree.m_alert == AlertStatus::Alert;
    facts.m_provoked           = provoked(world, entity);
    facts.m_hasTarget          = pool.m_target != entity && ecs.tryGet<Transform>(pool.m_target) != nullptr;
    facts.m_targetInSight      = row && inSight(world, entity, *row);
    facts.m_targetOutsideChase = row && facts.m_hasTarget && outsideChase(world, entity, pool.m_target, *row);
    facts.m_skillReady         = skillReady(pool, row);
    facts.m_movingRule         = movingRule(state);
    facts.m_lastAction         = facts.m_movingRule >= 0 ? AiAction::Idle : finishedAction(state);
    facts.m_hpPercent          = hpPercent(vitals);
    facts.m_inAttack           = state.isAttack();
    return facts;
}

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
