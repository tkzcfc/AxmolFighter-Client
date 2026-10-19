#pragma once

#include "mugen/core/math/Fixed.h"
#include <array>
#include <cstdint>
#include <span>

namespace mugen
{
class Random;
namespace sim
{
namespace ai
{
// 怪物条件表使用的原子判断。ChancePercent 必须是一行的最后一项。
enum class AiCondition : uint8_t
{
    // 恒真。
    Always,
    // 决策已进入警戒。
    Alerted,
    // 正在受击、持有仇恨或被嘲讽。
    Provoked,
    // 技能池锁定了其他角色。
    HasTarget,
    // 当前目标或候选者落在索敌范围。
    TargetInSight,
    // 当前目标落在追击范围外。
    TargetOutsideChase,
    // 自动施放间隔已经结束，或连段技能仍在等待续接。
    SkillReady,
    // 上次动作等于 m_value 中的 AiAction。
    LastActionIs,
    // 生命百分比严格低于 m_value。
    HpBelowPercent,
    // 闭区间 [1, 100] 的随机数不超过 m_value。
    ChancePercent,
};
// 一条规则中的单个判断。
struct AiTest
{
    // 判断种类。
    AiCondition m_kind = AiCondition::Always;
    // 为真时取反本项结果。
    bool m_negate = false;
    // 阈值、概率或动作编号，含义由 m_kind 决定。
    int32_t m_value = 0;
};
// 规则命中后要执行的动作。
enum class AiAction : uint8_t
{
    // 待机。
    Idle,
    // 槽位 X 的醒技能。
    CastWakeSkill,
    // 槽位 Z 的待机技能。
    CastIdleSkill,
    // 按优先级选择普通技能。
    CastBestSkill,
    // 追击到目标近侧。
    Chase,
    // 未警戒巡逻。
    Patrol,
    // 警戒范围内游走。
    Wander,
};
// 按顺序匹配的一行条件、动作和冷却配置。
struct AiRule
{
    // 最多三项判断，有效个数由 m_testCount 给出。
    std::array<AiTest, 3> m_tests{};
    // 本行实际参与判断的项数。
    uint8_t m_testCount = 0;
    // 命中后执行的动作。
    AiAction m_action = AiAction::Idle;
    // 命中后是否把警戒置为已警戒。
    bool m_setsAlert = false;
    // 命中后写入的冷却毫秒；0 表示无冷却。
    int32_t m_cooldownMs = 0;
};
// 组装一行规则；未计入 m_testCount 的项不参与判断。
constexpr AiRule makeRule(AiTest first,
                          AiTest second,
                          AiTest third,
                          uint8_t count,
                          AiAction action,
                          bool setsAlert = false)
{
    AiRule rule;
    rule.m_tests     = std::array<AiTest, 3>{first, second, third};
    rule.m_testCount = count;
    rule.m_action    = action;
    rule.m_setsAlert = setsAlert;
    return rule;
}
// 怪物默认条件表，按优先级从高到低排列。
inline constexpr std::array<AiRule, 8> kMonsterRules = {
    // 未警戒且被激怒时释放醒技能，并进入警戒。
    makeRule({AiCondition::Alerted, true}, {AiCondition::Provoked}, {}, 2, AiAction::CastWakeSkill, true),
    // 未警戒且索敌范围内发现目标时释放醒技能，并进入警戒。
    makeRule({AiCondition::Alerted, true}, {AiCondition::TargetInSight}, {}, 2, AiAction::CastWakeSkill, true),
    // 未警戒且上次动作是巡逻时释放待机技能。
    makeRule({AiCondition::Alerted, true},
             {AiCondition::LastActionIs, false, static_cast<int32_t>(AiAction::Patrol)},
             {},
             2,
             AiAction::CastIdleSkill),
    // 未警戒时巡逻。
    makeRule({AiCondition::Alerted, true}, {}, {}, 1, AiAction::Patrol),
    // 已警戒、有目标且技能间隔就绪时释放最优技能。
    makeRule({AiCondition::Alerted}, {AiCondition::HasTarget}, {AiCondition::SkillReady}, 3, AiAction::CastBestSkill),
    // 已警戒、有目标且目标在追击范围外时追击。
    makeRule({AiCondition::Alerted}, {AiCondition::HasTarget}, {AiCondition::TargetOutsideChase}, 3, AiAction::Chase),
    // 已警戒且有目标时在范围内游走。
    makeRule({AiCondition::Alerted}, {AiCondition::HasTarget}, {}, 2, AiAction::Wander),
    // 其余情况待机。
    makeRule({AiCondition::Always}, {}, {}, 1, AiAction::Idle),
};
// 条件判断使用的只读快照。
struct AiFacts
{
    // 决策警戒已打开。
    bool m_alerted = false;
    // 受击、仇恨或嘲讽仍然有效。
    bool m_provoked = false;
    // 技能池的目标不是自身哨兵。
    bool m_hasTarget = false;
    // 索敌范围内能看到目标或候选者。
    bool m_targetInSight = false;
    // 当前目标不在追击范围内。
    bool m_targetOutsideChase = false;
    // 至少一个自动施放间隔已经到点，或连段技能仍在等待续接。
    bool m_skillReady = false;
    // 上一次已经结束的动作；正在跑的移动不算。
    AiAction m_lastAction = AiAction::Idle;
    // 当前生命占上限的百分比，向零截断。
    int32_t m_hpPercent = 0;
    // 正在执行的移动规则下标；-1 表示没有移动在跑。
    int8_t m_movingRule = -1;
    // 角色正处于攻击状态。
    bool m_inAttack = false;
};
// 返回第一条命中且未在冷却的规则下标；攻击中或没有更高优先级可抢占时返回 -1。
int8_t pickRule(const AiFacts& facts,
                std::span<const AiRule> rules,
                std::span<const Fixed> ruleCooldownMs,
                Random& rng);
// 把每行剩余冷却减去一帧时长，不会减到零以下。
void tickRuleCooldowns(std::span<Fixed> ruleCooldownMs, Fixed dtMs);
}  // namespace ai
}  // namespace sim
}  // namespace mugen
