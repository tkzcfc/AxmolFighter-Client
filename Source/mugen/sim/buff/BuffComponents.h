#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"
#include <cstdint>
#include <vector>

namespace mugen::sim
{
// 锁血下限在 StatsComponent.mods 中使用的内部属性号（不在 BuffStat 枚举里）。
int32_t constexpr kHpLockStat = 300;

// 一次准入或触发携带的上下文。施加者为空时视为接收者自身。
struct BuffContext
{
    // 施加者。
    Entity applicator;
    // 事件目标。
    Entity target;
    // 事件技能。
    int32_t skillId = -1;
    // 绑定技能；准入后不随事件覆盖。
    int32_t boundSkill = -1;
    // 命中特效配置。
    int32_t effectId = 0;
    // 本次伤害。
    Fixed damage;
    // 本次扣费的 MP。
    Fixed tpSpent;
    // 是否暴击。
    bool critical = false;
    // 事件目标是否已死。
    bool targetDead = false;
};

// 添加冷却。
struct BuffCd
{
    // Buff 配置标识。
    int32_t id = 0;
    // 剩余秒数。
    Fixed left;
    MG_REFLECT(BuffCd, id, left)
};

// 按来源登记的准入免疫。
struct BuffImmune
{
    // ImmuneSel 数值。
    int32_t sel = 0;
    // 规则 id 或 buffType；All 时为 0。
    int32_t key = 0;
    // 来源实例序号。
    uint32_t source = 0;
    MG_REFLECT(BuffImmune, sel, key, source)
};

// 一个 Buff 实例。常驻 op 不拷贝，失活时按序号撤销。
struct BuffInstance
{
    // 实例序号。
    uint32_t serial = 0;
    // 当前配置标识。
    int32_t id = 0;
    // 首次准入的配置标识。
    int32_t initialId = 0;
    // 层数。
    int32_t stacks = 1;
    // 已执行次数。
    int32_t times = 0;
    // 配置次数上限。
    int32_t maxTimes = 0;
    // 当前周期已走秒数。
    Fixed interval;
    // 配置周期秒数。
    Fixed maxInterval;
    // 内部冷却已走秒数。
    Fixed innerElapsed;
    // 内部冷却秒数。
    Fixed innerCd;
    // 已激活（等待事件的实例为假）。
    bool active = false;
    // 常驻 op 门闩。
    bool latched = false;
    // 逻辑已移除，等待从列表擦掉。
    bool removing = false;
    // 施加者。
    Entity applicator;
    // 最近一次事件的目标。
    Entity eventTarget;
    // 绑定技能。
    int32_t boundSkill = -1;
    // 最近一次事件技能。
    int32_t skillId = -1;
    // 最近一次命中特效。
    int32_t effectId = 0;
    // 最近一次伤害。
    Fixed damage;
    // 最近一次扣费 MP。
    Fixed tpSpent;
    // 最近一次是否暴击。
    bool critical = false;
    // 最近一次目标是否已死。
    bool targetDead = false;
    // Choice 选中的分支；−1 表示还没选。
    int32_t choice = -1;
    // 顺序挑选游标。
    int32_t cursor = 0;
    // Watch 上一次结果的位掩码。
    uint32_t watchBits = 0;
    // 三类受击计时已经越过阈值。
    bool getUpDone = false;
    bool repelDone = false;
    bool flyDone = false;
    // 三类受击状态累计秒数。
    Fixed getUpSec;
    Fixed repelSec;
    Fixed flySec;
    MG_REFLECT(BuffInstance,
               serial,
               id,
               initialId,
               stacks,
               times,
               maxTimes,
               interval,
               maxInterval,
               innerElapsed,
               innerCd,
               active,
               latched,
               removing,
               applicator,
               eventTarget,
               boundSkill,
               skillId,
               effectId,
               damage,
               tpSpent,
               critical,
               targetDead,
               choice,
               cursor,
               watchBits,
               getUpDone,
               repelDone,
               flyDone,
               getUpSec,
               repelSec,
               flySec)
};

// 角色持有的 Buff 列表。
struct BuffListComponent
{
    // 全部实例。
    std::vector<BuffInstance> instances;
    // 下一个实例序号。
    uint32_t nextSerial = 1;
    // 添加冷却。
    std::vector<BuffCd> cooldowns;
    // 免疫登记。
    std::vector<BuffImmune> immunity;
    MG_REFLECT(BuffListComponent, instances, nextSerial, cooldowns, immunity)
};

// 一条特殊能力实例。
struct AbilityInstance
{
    // 实例序号。
    uint32_t serial = 0;
    // 配置标识。
    int32_t id = 0;
    // 剩余冷却秒数。
    Fixed cdLeft;
    // 周期已走秒数。
    Fixed elapsed;
    // 结束倒计时；负值表示不结束。
    Fixed endLeft;
    // 施加者。
    Entity applicator;
    MG_REFLECT(AbilityInstance, serial, id, cdLeft, elapsed, endLeft, applicator)
};

// 角色持有的特殊能力。
struct AbilityListComponent
{
    // 全部实例。
    std::vector<AbilityInstance> instances;
    // 下一个序号。
    uint32_t nextSerial = 1;
    MG_REFLECT(AbilityListComponent, instances, nextSerial)
};

// 嘲讽权重。
struct TauntEntry
{
    // 施加者。
    Entity applicator;
    // 权重。
    Fixed weight;
    // 来源实例序号。
    uint32_t source = 0;
    MG_REFLECT(TauntEntry, applicator, weight, source)
};

// 接收者身上的嘲讽表。
struct TauntTableComponent
{
    // 按首次出现顺序。
    std::vector<TauntEntry> entries;
    MG_REFLECT(TauntTableComponent, entries)
};

// 连击与评价状态，供 ComboTable、Buff 条件与战斗结算读取。
struct ComboMeterComponent
{
    // 当前评价等级（0 表示无评价，1..N 对应评价阶段）。
    int32_t grade = 0;
    // 当前连击数。
    int32_t count = 0;
    // 本局最高连击数。
    int32_t maxCount = 0;
    // 本局最高评价等级。
    int32_t maxGrade = 0;
    // 当前评价经验。
    Fixed exp;
    // 当前连击超时计时（毫秒）。
    Fixed timerMs;
    // 当前连击段内已结算过击杀经验的目标实体。
    std::vector<Entity> killedTargets;
    MG_REFLECT(ComboMeterComponent, grade, count, maxCount, maxGrade, exp, timerMs, killedTargets)
};

// 怒气。
struct RageStateComponent
{
    // 当前怒气。
    Fixed rage;
    // 冻结剩余毫秒。
    Fixed freezeMs;
    // 冻结上限。
    Fixed maxFreezeMs;
    // 是否处于变身。
    bool transformed = false;
    MG_REFLECT(RageStateComponent, rage, freezeMs, maxFreezeMs, transformed)
};
}  // namespace mugen::sim
