#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/World.h"
#include <algorithm>

namespace mugen::sim::health::rules
{
// 扣血或回血。delta 为负表示扣减。
void applyDelta(World& world, Entity entity, Fixed delta, HpCause cause, Entity source, Entity effect)
{
    auto& health       = world.ecs().get<Health>(entity);
    const Fixed before = health.m_hp;
    health.m_hp        = std::clamp(before + delta, Fixed{}, health.m_maxHp);
    HpEvent hpEvent;
    hpEvent.m_entity = entity;
    hpEvent.m_delta  = delta;
    hpEvent.m_cause  = cause;
    hpEvent.m_source = source;
    world.ecs().singleton<EventQueue<HpEvent>>().push(hpEvent);
    // 仍≤0 才立即判死并请求死亡；同一实体只请求一次。
    if (health.m_hp <= Fixed{} && health.m_maxHp > Fixed{} && !health.m_dead)
    {
        health.m_dead = true;
        DeathRequest request;
        request.m_entity = entity;
        request.m_killer = source;
        request.m_effect = effect;
        world.ecs().singleton<EventQueue<DeathRequest>>().push(request);
    }
}

// 已打上死亡标记，或生命耗尽且具备有效上限。
bool isDead(const Health& health)
{
    return health.m_dead || health.isDead();
}
}  // namespace mugen::sim::health::rules
