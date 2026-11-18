#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include <algorithm>

namespace mugen::sim::health::rules
{
// 扣血或回血。delta 为负表示扣减。ignoreShield 为真时扣血不经过护盾。
void applyDelta(World& world, Entity entity, Fixed delta, HpCause cause, Entity source, Entity effect, bool ignoreShield)
{
    auto& health = world.ecs().get<HealthComponent>(entity);
    if (cause == HpCause::Heal && world.ecs().has<StatusFlagsComponent>(entity) &&
        world.ecs().get<StatusFlagsComponent>(entity).noHeal > 0)
        return;
    if (delta > Fixed{} && cause == HpCause::Heal && world.ecs().has<StatsComponent>(entity))
        delta *= Fixed::fromInt(1) + world.ecs().get<StatsComponent>(entity).finalStat(208);
    if (delta < Fixed{} && !ignoreShield && health.shield > Fixed{})
    {
        const Fixed absorb = std::min(health.shield, -delta);
        health.shield -= absorb;
        delta += absorb;
        if (absorb > Fixed{})
        {
            HpEvent shieldEvent;
            shieldEvent.entity = entity;
            shieldEvent.delta  = absorb;
            shieldEvent.cause  = HpCause::Shield;
            shieldEvent.source = source;
            world.ecs().singleton<EventQueue<HpEvent>>().push(shieldEvent);
        }
        if (health.shield <= Fixed{})
        {
            health.shield = {};
            BuffContext ctx;
            ctx.target = entity;
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::ShieldClean, ctx);
        }
    }
    if (world.ecs().has<StatsComponent>(entity))
        health.maxHp = world.ecs().get<StatsComponent>(entity).maxHp();
    const Fixed before = health.hp;
    Fixed floor{};
    if (world.ecs().has<StatsComponent>(entity))
        floor = std::max(Fixed{}, world.ecs().get<StatsComponent>(entity).finalStat(300));
    Fixed hi = health.maxHp;
    if (hi < floor)
        hi = floor;
    health.hp = std::clamp(before + delta, floor, hi);
    const Fixed applied = health.hp - before;
    if (applied != Fixed{})
    {
        HpEvent hpEvent;
        hpEvent.entity = entity;
        hpEvent.delta  = applied;
        hpEvent.cause  = cause;
        hpEvent.source = source;
        world.ecs().singleton<EventQueue<HpEvent>>().push(hpEvent);
    }
    if (health.hp <= Fixed{} && !health.dead)
    {
        BuffContext ctx;
        ctx.target = entity;
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeDeath, ctx);
    }
    auto& after = world.ecs().get<HealthComponent>(entity);
    if (after.hp <= Fixed{} && after.maxHp > Fixed{} && !after.dead)
    {
        after.dead = true;
        DeathRequest request;
        request.entity = entity;
        request.killer = source;
        request.effect = effect;
        world.ecs().singleton<EventQueue<DeathRequest>>().push(request);
    }
    else if (!after.dead && after.hp != before)
    {
        BuffContext ctx;
        ctx.target = entity;
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::HpChange, ctx);
    }
}

// 已打上死亡标记，或生命耗尽且具备有效上限。
bool isDead(const HealthComponent& health)
{
    return health.dead || health.isDead();
}
}  // namespace mugen::sim::health::rules
