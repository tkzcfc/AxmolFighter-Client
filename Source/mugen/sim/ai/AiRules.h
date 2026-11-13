#pragma once

#include "mugen/core/math/Fixed.h"
#include "mugen/core/ecs/Entity.h"
#include <array>
#include <cstdint>
#include <span>

namespace mugen
{
namespace sim
{
class World;

namespace ai
{
// 怪物条件表使用的原子判断。ChancePercent 必须是一行的最后一项。
enum class AiCondition : uint8_t
{
    Always,             // 恒真。
    Alerted,            // 决策已进入警戒。
    Provoked,           // 正在受击、持有仇恨或被嘲讽。
    HasTarget,          // 技能池锁定了其他角色。
    TargetInSight,      // 当前目标或候选者落在索敌范围。
    TargetOutsideChase, // 当前目标落在追击范围外。
    SkillReady,         // 自动施放间隔已经结束，或连段技能仍在等待续接。
    LastActionIs,       // 上次动作等于 m_value 中的 AiAction。
    HpBelowPercent,     // 生命百分比严格低于 m_value。
    ChancePercent,      // 闭区间 [1, 100] 的随机数不超过 m_value。
};
// 一条规则中的单个判断。
struct AiTest
{
    AiCondition m_kind = AiCondition::Always; // 判断种类。
    bool m_negate = false;                    // 为真时取反本项结果。
    int32_t m_value = 0;                      // 阈值、概率或动作编号，含义由 m_kind 决定。
};
// 规则命中后要执行的动作。
enum class AiAction : uint8_t
{
    Idle,            // 待机。
    CastWakeSkill,   // 槽位醒技能。
    CastIdleSkill,   // 槽位待机技能。
    CastBestSkill,   // 按优先级选择普通技能。
    Chase,           // 追击到目标近侧。
    Patrol,          // 未警戒巡逻。
    Wander,          // 警戒范围内游走。
};
// 按顺序匹配的一行条件、动作和冷却配置。
struct AiRule
{
    std::array<AiTest, 3> m_tests{}; // 最多三项判断，有效个数由 m_testCount 给出。
    uint8_t m_testCount = 0;         // 本行实际参与判断的项数。
    AiAction m_action = AiAction::Idle; // 命中后执行的动作。
    bool m_setsAlert = false;           // 命中后是否把警戒置为已警戒。
    int32_t m_cooldownMs = 0;           // 命中后写入的冷却毫秒；0 表示无冷却。
};
// 组装一行规则；未计入 m_testCount 的项不参与判断。
constexpr AiRule makeRule(AiTest first, AiTest second, AiTest third, uint8_t count, AiAction action,
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
    makeRule({AiCondition::Alerted, true}, {AiCondition::Provoked}, {}, 2, AiAction::CastWakeSkill, true),
    makeRule({AiCondition::Alerted, true}, {AiCondition::TargetInSight}, {}, 2, AiAction::CastWakeSkill, true),
    makeRule({AiCondition::Alerted, true},
             {AiCondition::LastActionIs, false, static_cast<int32_t>(AiAction::Patrol)},
             {},
             2,
             AiAction::CastIdleSkill),
    makeRule({AiCondition::Alerted, true}, {}, {}, 1, AiAction::Patrol),
    makeRule({AiCondition::Alerted}, {AiCondition::HasTarget}, {AiCondition::SkillReady}, 3, AiAction::CastBestSkill),
    makeRule({AiCondition::Alerted}, {AiCondition::HasTarget}, {AiCondition::TargetOutsideChase}, 3, AiAction::Chase),
    makeRule({AiCondition::Alerted}, {AiCondition::HasTarget}, {}, 2, AiAction::Wander),
    makeRule({AiCondition::Always}, {}, {}, 1, AiAction::Idle),
};
// 条件判断使用的只读快照。
struct AiFacts
{
    bool m_alerted = false;            // 决策警戒已打开。
    bool m_provoked = false;           // 受击、仇恨或嘲讽仍然有效。
    bool m_hasTarget = false;          // 技能池的目标不是自身哨兵。
    bool m_targetInSight = false;      // 索敌范围内能看到目标或候选者。
    bool m_targetOutsideChase = false; // 当前目标不在追击范围内。
    bool m_skillReady = false;         // 至少一个自动施放间隔已经到点，或连段技能仍在等待续接。
    AiAction m_lastAction = AiAction::Idle; // 上一次已经结束的动作；正在跑的移动不算。
    int32_t m_hpPercent = 0;                // 当前生命占上限的百分比，向零截断。
    int8_t m_movingRule = -1;               // 正在执行的移动规则下标；-1 表示没有移动在跑。
    bool m_inAttack = false;                // 角色正处于攻击状态。
};

namespace rules
{
// 返回第一条命中且未在冷却的规则下标；攻击中或没有更高优先级可抢占时返回 -1。
int8_t pickRule(World& world, const AiFacts& facts, std::span<const AiRule> rules,
                std::span<const Fixed> ruleCooldownMs);
// 把每行剩余冷却减去一帧时长，不会减到零以下。
void tickRuleCooldowns(std::span<Fixed> ruleCooldownMs, Fixed dtMs);
// 单角色 AI 决策入口：减冷却 → 选目标 → 采集事实 → 选规则 → 执行动作写 IntentComponent。
void updateAgent(World& world, Entity entity);
// 记录命中伤害（供 Combat 调用，不写非 AI 组件）。
void addThreat(World& world, Entity entity, Entity source, Fixed damage);
// 保证角色持有 AiBrain 组件与怪物技能簿（出生时懒初始化）。
// aiDifficulty 为 1-based 难度下标，按 RoleDef.aiId 解析，越界回退最后一项。
void ensureBrain(World& world, Entity entity, int32_t aiDifficulty = 1);
}  // namespace rules
}  // namespace ai
}  // namespace sim
}  // namespace mugen