#pragma once

#include "BuffLifecycle.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/conf/GameDef.h"

namespace mugen::sim
{
// 由连击和伙伴系统提供的条件输入，不在 Buff 内维护另一套玩法状态。
struct BuffExternalConditions
{
    // 当前连击评价等级。
    int32_t m_comboEvaluate = 0;
    // 是否属于怒气变身伙伴及其剩余变身时间。
    bool m_angerPartner = false;
    Fixed m_angerFreezeSeconds;
    MG_REFLECT(BuffExternalConditions, m_comboEvaluate, m_angerPartner, m_angerFreezeSeconds)
};
// 实例事件上下文，实体之间仅保存代际句柄。
struct BuffContext
{
    // 当前目标、施加者和施加技能属性来源。
    Entity m_target, m_applicator, m_skillAttribute;
    // 绑定、当前及前置技能配置标识。
    std::optional<int32_t> m_boundSkill, m_currentSkill, m_frontSkill;
    // 神器与动作来源标识。
    std::optional<int32_t> m_artifactId, m_artifactIndex, m_actionId;
    // 本次消耗与伤害信息。
    Fixed m_needUseTp, m_damage;
    // 触发事件携带的暴击标记。
    bool m_critical = false;
    // 外部装配指定的来源分类，事件触发不修改实例来源。
    std::optional<int32_t> m_origin;
    MG_REFLECT(BuffContext,
               m_target,
               m_applicator,
               m_skillAttribute,
               m_boundSkill,
               m_currentSkill,
               m_frontSkill,
               m_artifactId,
               m_artifactIndex,
               m_actionId,
               m_needUseTp,
               m_damage,
               m_critical,
               m_origin)
};

// 按插入顺序存储的 Buff 值实例，稳定序号供回调后重新定位。
struct BuffInstance
{
    // 单实体池内单调递增的实例标识。
    uint64_t m_serial = 0;
    // 当前配置标识，重置时允许更换。
    int32_t m_id = 0;
    // 初始化配置标识保存不会随重置刷新的事件、层数上限和移层策略。
    int32_t m_initialId = 0;
    // 生命周期计时和层数的唯一存储。
    buff::Lifecycle m_lifecycle;
    // 最近一次事件与施加来源。
    BuffContext m_context;
    // 原始来源分类，未提供时保持空值。
    std::optional<int32_t> m_source;
    // 规则事件门闩及当前已施加的状态引用数。
    bool m_triggered      = false;
    int32_t m_appliedRefs = 0;
    // 规则进入时计算的数值，用于按进入时快照精确撤销。
    Fixed m_ruleValue;
    // 状态施加器最近收到的行为类名与开始结束事件。
    std::string m_behaviorAction;
    BFEvent m_behaviorEvent = BFEvent::BehaviorStateEnd;
    MG_REFLECT(BuffInstance,
               m_serial,
               m_id,
               m_initialId,
               m_lifecycle,
               m_context,
               m_source,
               m_triggered,
               m_appliedRefs,
               m_ruleValue,
               m_behaviorAction,
               m_behaviorEvent)
};

// Buff 添加冷却条目；零值仍保留至下一次冷却更新。
struct BuffCooldown
{
    // 冷却所属配置标识。
    int32_t m_id = 0;
    // 剩余时间，单位秒。
    Fixed m_seconds;
    MG_REFLECT(BuffCooldown, m_id, m_seconds)
};

// 单实体拥有的完整 Buff 池，不保存对象指针或运行时回调。
struct BuffPool
{
    // 实例数组按插入顺序遍历和序列化。
    std::vector<BuffInstance> m_instances;
    // 免疫规则主键集合，负一表示全部免疫。
    std::vector<int32_t> m_immunity;
    // 添加冷却数组保留插入顺序。
    std::vector<BuffCooldown> m_cooldowns;
    // 各规则引用计数使用配置主键而不是稠密枚举。
    std::map<int32_t, int32_t> m_ruleCounters;
    // 下一个实例标识，零值不分配。
    uint64_t m_nextSerial = 1;
    MG_REFLECT(BuffPool, m_instances, m_immunity, m_cooldowns, m_ruleCounters, m_nextSerial)

    // 更新添加冷却，调用者明确传入秒而非毫秒。
    void updateCooldowns(Fixed dtSeconds);
    // 增删规则免疫集合，重复添加不累积引用。
    void setImmune(int32_t ruleId, bool immune);
    // 根据现有同类层数计算概率，严格小于判定消耗调用方提供的抽样。
    bool probabilityPass(const conf::BuffConfig& config, Fixed draw) const;
    // 按配置规则和子类型返回首个实例的稳定序号。
    std::optional<uint64_t> findByType(int32_t ruleId, int32_t subType) const;
};
}  // namespace mugen::sim
