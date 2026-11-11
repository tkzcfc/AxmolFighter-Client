#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"
#include <cstdint>
#include <vector>

namespace mugen::sim
{
// 锁血下限在 Stats.m_mods 中使用的内部属性号（不在 BuffStat 枚举里）。
int32_t constexpr kHpLockStat = 300;

// 一次准入或触发携带的上下文。施加者为空时视为接收者自身。
struct BuffContext
{
    // 施加者。
    Entity m_applicator;
    // 事件目标。
    Entity m_target;
    // 事件技能。
    int32_t m_skillId = -1;
    // 绑定技能；准入后不随事件覆盖。
    int32_t m_boundSkill = -1;
    // 命中特效配置。
    int32_t m_effectId = 0;
    // 本次伤害。
    Fixed m_damage;
    // 本次扣费的 MP。
    Fixed m_tpSpent;
    // 是否暴击。
    bool m_critical = false;
    // 事件目标是否已死。
    bool m_targetDead = false;
};

// 添加冷却。
struct BuffCd
{
    // Buff 配置标识。
    int32_t m_id = 0;
    // 剩余秒数。
    Fixed m_left;
    MG_REFLECT(BuffCd, m_id, m_left)
};

// 按来源登记的准入免疫。
struct BuffImmune
{
    // ImmuneSel 数值。
    int32_t m_sel = 0;
    // 规则 id 或 buffType；All 时为 0。
    int32_t m_key = 0;
    // 来源实例序号。
    uint32_t m_source = 0;
    MG_REFLECT(BuffImmune, m_sel, m_key, m_source)
};

// 一个 Buff 实例。常驻 op 不拷贝，失活时按序号撤销。
struct BuffInstance
{
    // 实例序号。
    uint32_t m_serial = 0;
    // 当前配置标识。
    int32_t m_id = 0;
    // 首次准入的配置标识。
    int32_t m_initialId = 0;
    // 层数。
    int32_t m_stacks = 1;
    // 已执行次数。
    int32_t m_times = 0;
    // 配置次数上限。
    int32_t m_maxTimes = 0;
    // 当前周期已走秒数。
    Fixed m_interval;
    // 配置周期秒数。
    Fixed m_maxInterval;
    // 内部冷却已走秒数。
    Fixed m_innerElapsed;
    // 内部冷却秒数。
    Fixed m_innerCd;
    // 已激活（等待事件的实例为假）。
    bool m_active = false;
    // 常驻 op 门闩。
    bool m_latched = false;
    // 逻辑已移除，等待从列表擦掉。
    bool m_removing = false;
    // 施加者。
    Entity m_applicator;
    // 最近一次事件的目标。
    Entity m_eventTarget;
    // 绑定技能。
    int32_t m_boundSkill = -1;
    // 最近一次事件技能。
    int32_t m_skillId = -1;
    // 最近一次命中特效。
    int32_t m_effectId = 0;
    // 最近一次伤害。
    Fixed m_damage;
    // 最近一次扣费 MP。
    Fixed m_tpSpent;
    // 最近一次是否暴击。
    bool m_critical = false;
    // 最近一次目标是否已死。
    bool m_targetDead = false;
    // Choice 选中的分支；−1 表示还没选。
    int32_t m_choice = -1;
    // 顺序挑选游标。
    int32_t m_cursor = 0;
    // Watch 上一次结果的位掩码。
    uint32_t m_watchBits = 0;
    // 三类受击计时已经越过阈值。
    bool m_getUpDone = false;
    bool m_repelDone = false;
    bool m_flyDone = false;
    // 三类受击状态累计秒数。
    Fixed m_getUpSec;
    Fixed m_repelSec;
    Fixed m_flySec;
    MG_REFLECT(BuffInstance,
               m_serial,
               m_id,
               m_initialId,
               m_stacks,
               m_times,
               m_maxTimes,
               m_interval,
               m_maxInterval,
               m_innerElapsed,
               m_innerCd,
               m_active,
               m_latched,
               m_removing,
               m_applicator,
               m_eventTarget,
               m_boundSkill,
               m_skillId,
               m_effectId,
               m_damage,
               m_tpSpent,
               m_critical,
               m_targetDead,
               m_choice,
               m_cursor,
               m_watchBits,
               m_getUpDone,
               m_repelDone,
               m_flyDone,
               m_getUpSec,
               m_repelSec,
               m_flySec)
};

// 角色持有的 Buff 列表。
struct BuffList
{
    // 全部实例。
    std::vector<BuffInstance> m_instances;
    // 下一个实例序号。
    uint32_t m_nextSerial = 1;
    // 添加冷却。
    std::vector<BuffCd> m_cooldowns;
    // 免疫登记。
    std::vector<BuffImmune> m_immunity;
    MG_REFLECT(BuffList, m_instances, m_nextSerial, m_cooldowns, m_immunity)
};

// 一条特殊能力实例。
struct AbilityInstance
{
    // 实例序号。
    uint32_t m_serial = 0;
    // 配置标识。
    int32_t m_id = 0;
    // 剩余冷却秒数。
    Fixed m_cdLeft;
    // 周期已走秒数。
    Fixed m_elapsed;
    // 结束倒计时；负值表示不结束。
    Fixed m_endLeft;
    // 施加者。
    Entity m_applicator;
    MG_REFLECT(AbilityInstance, m_serial, m_id, m_cdLeft, m_elapsed, m_endLeft, m_applicator)
};

// 角色持有的特殊能力。
struct AbilityList
{
    // 全部实例。
    std::vector<AbilityInstance> m_instances;
    // 下一个序号。
    uint32_t m_nextSerial = 1;
    MG_REFLECT(AbilityList, m_instances, m_nextSerial)
};

// 嘲讽权重。
struct TauntEntry
{
    // 施加者。
    Entity m_applicator;
    // 权重。
    Fixed m_weight;
    // 来源实例序号。
    uint32_t m_source = 0;
    MG_REFLECT(TauntEntry, m_applicator, m_weight, m_source)
};

// 接收者身上的嘲讽表。
struct TauntTable
{
    // 按首次出现顺序。
    std::vector<TauntEntry> m_entries;
    MG_REFLECT(TauntTable, m_entries)
};

// 连击评价，供 ComboTable 读取。
struct ComboMeter
{
    // 评价等级。
    int32_t m_grade = 0;
    // 连击数。
    int32_t m_count = 0;
    MG_REFLECT(ComboMeter, m_grade, m_count)
};

// 怒气。
struct RageState
{
    // 当前怒气。
    Fixed m_rage;
    // 冻结剩余毫秒。
    Fixed m_freezeMs;
    // 冻结上限。
    Fixed m_maxFreezeMs;
    // 是否处于变身。
    bool m_transformed = false;
    MG_REFLECT(RageState, m_rage, m_freezeMs, m_maxFreezeMs, m_transformed)
};
}  // namespace mugen::sim
