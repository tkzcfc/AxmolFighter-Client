#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim
{
// 特殊能力事件携带的命中、技能和目标上下文。
struct AbilityContext
{
    // 本次事件关联的目标和施放者。
    Entity m_target, m_applicator;
    // 技能配置标识，负一表示事件没有技能来源。
    int32_t m_skillId = -1;
    // 本次事件是否为暴击。
    bool m_critical = false;
    MG_REFLECT(AbilityContext, m_target, m_applicator, m_skillId, m_critical)
};

// 单个特殊能力的可快照运行状态。
struct AbilityInstance
{
    // 实体能力池内单调递增的稳定标识。
    uint64_t m_serial = 0;
    // special_ability 配置标识。
    int32_t m_id = 0;
    // 可选的绑定技能配置标识。
    std::optional<int32_t> m_boundSkill;
    // 最近触发事件的上下文。
    AbilityContext m_context;
    // 当前冷却、周期和结束倒计时，单位秒。
    Fixed m_cooldown, m_interval, m_ended;
    // 是否已经执行过启动动作。
    bool m_active = false;
    MG_REFLECT(AbilityInstance, m_serial, m_id, m_boundSkill, m_context, m_cooldown, m_interval, m_ended, m_active)
};

// 单实体特殊能力池，按插入顺序分派和序列化。
struct AbilityPool
{
    // 所有能力值实例，回调期间通过稳定标识重新定位。
    std::vector<AbilityInstance> m_instances;
    // 下一个非零稳定标识。
    uint64_t m_nextSerial = 1;
    MG_REFLECT(AbilityPool, m_instances, m_nextSerial)
};
}  // namespace mugen::sim
