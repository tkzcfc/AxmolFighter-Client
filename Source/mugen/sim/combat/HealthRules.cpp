#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include <algorithm>

namespace mugen::sim::health::rules
{
// 扣血或回血。delta 为负表示扣减。ignoreShield 为真时扣血不经过护盾。
void applyDelta(World& world, Entity entity, Fixed delta, HpCause cause, Entity source, Entity effect, bool ignoreShield)
{
    auto& health = world.ecs().get<Health>(entity);
    if (cause == HpCause::Heal && world.ecs().has<StatusFlags>(entity) &&
        world.ecs().get<StatusFlags>(entity).m_noHeal > 0)
        return;
    if (delta > Fixed{} && cause == HpCause::Heal && world.ecs().has<Stats>(entity))
        delta *= Fixed::fromInt(1) + world.ecs().get<Stats>(entity).finalStat(208);
    if (delta < Fixed{} && !ignoreShield && health.m_shield > Fixed{})
    {
        const Fixed absorb = std::min(health.m_shield, -delta);
        health.m_shield -= absorb;
        delta += absorb;
        if (absorb > Fixed{})
        {
            HpEvent shieldEvent;
            shieldEvent.m_entity = entity;
            shieldEvent.m_delta  = absorb;
            shieldEvent.m_cause  = HpCause::Shield;
            shieldEvent.m_source = source;
            world.ecs().singleton<EventQueue<HpEvent>>().push(shieldEvent);
        }
        if (health.m_shield <= Fixed{})
        {
            health.m_shield = {};
            BuffContext ctx;
            ctx.m_target = entity;
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::ShieldClean, ctx);
        }
    }
    if (world.ecs().has<Stats>(entity))
        health.m_maxHp = world.ecs().get<Stats>(entity).maxHp();
    const Fixed before = health.m_hp;
    Fixed floor{};
    if (world.ecs().has<Stats>(entity))
        floor = std::max(Fixed{}, world.ecs().get<Stats>(entity).finalStat(300));
    Fixed hi = health.m_maxHp;
    if (hi < floor)
        hi = floor;
    health.m_hp = std::clamp(before + delta, floor, hi);
    const Fixed applied = health.m_hp - before;
    if (applied != Fixed{})
    {
        HpEvent hpEvent;
        hpEvent.m_entity = entity;
        hpEvent.m_delta  = applied;
        hpEvent.m_cause  = cause;
        hpEvent.m_source = source;
        world.ecs().singleton<EventQueue<HpEvent>>().push(hpEvent);
    }
    if (health.m_hp <= Fixed{} && !health.m_dead)
    {
        BuffContext ctx;
        ctx.m_target = entity;
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeDeath, ctx);
    }
    auto& after = world.ecs().get<Health>(entity);
    if (after.m_hp <= Fixed{} && after.m_maxHp > Fixed{} && !after.m_dead)
    {
        after.m_dead = true;
        DeathRequest request;
        request.m_entity = entity;
        request.m_killer = source;
        request.m_effect = effect;
        world.ecs().singleton<EventQueue<DeathRequest>>().push(request);
    }
    else if (!after.m_dead && after.m_hp != before)
    {
        BuffContext ctx;
        ctx.m_target = entity;
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::HpChange, ctx);
    }
}

// 已打上死亡标记，或生命耗尽且具备有效上限。
bool isDead(const Health& health)
{
    return health.m_dead || health.isDead();
}
}  // namespace mugen::sim::health::rules
