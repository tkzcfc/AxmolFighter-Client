#include "mugen/sim/combat/CombatRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/base/Rng.h"
#include "mugen/sim/combat/DamageRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim::combat::rules
{
namespace
{
// 标准伤害：英雄用物攻 + 等级标准，其他单位用基础伤害 + 十级分段标准。
Fixed standardHurt(World& world, Entity attacker)
{
    const auto& identity = world.ecs().get<Identity>(attacker);
    const auto& stats    = world.ecs().get<Stats>(attacker);
    const bool hero      = identity.m_roleType == 1;
    const int32_t level  = hero ? identity.m_level : std::max(1, identity.m_level / 10);
    const auto* row      = conf::Config::instance().find<conf::SkillHurtConfig>(level);
    if (!row)
        return {};
    return stats.value(hero ? 5 : 15) + Fixed::fromInt(hero ? row->hurt : row->monsterHurt);
}
// 发出顿帧事实。毫秒到帧的换算留给 DebugSink。
void emitHitStop(World& world, Entity entity, Fixed freezeMs)
{
    HitStopEvent event;
    event.m_entity   = entity;
    event.m_freezeMs = freezeMs;
    world.ecs().singleton<EventQueue<HitStopEvent>>().push(event);
}
}  // namespace

// 步骤 1：未销毁、主人存在、阵营敌对、间隔/次数/去重、未死且可受击。
bool gate(World& world, const ContactEvent& contact, HitContext& ctx)
{
    const Entity effect = contact.m_a;
    const Entity target = contact.m_b;
    auto& ecs           = world.ecs();
    if (!ecs.alive(effect) || ecs.has<Despawning>(effect) || !ecs.has<EffectInfo>(effect))
        return false;
    if (!ecs.alive(target) || ecs.has<Despawning>(target) || !ecs.has<Health>(target))
        return false;
    const auto& effectInfo = ecs.get<EffectInfo>(effect);
    const auto* effectRow  = conf::Config::instance().find<conf::battle::EffectDef>(effectInfo.m_configId);
    if (!effectRow || effectRow->hitId <= 0)
        return false;
    const auto* hitRow = conf::Config::instance().find<conf::battle::HitDef>(effectRow->hitId);
    if (!hitRow)
        return false;
    if (!ecs.alive(effectInfo.m_owner) || !ecs.has<Stats>(effectInfo.m_owner))
        return false;
    const auto& ownerIdentity  = ecs.get<Identity>(effectInfo.m_owner);
    const auto& targetIdentity = ecs.get<Identity>(target);
    if ((static_cast<uint32_t>(ownerIdentity.m_hostileMask) & static_cast<uint32_t>(targetIdentity.m_camp)) !=
        static_cast<uint32_t>(targetIdentity.m_camp))
        return false;

    const auto& tracker      = ecs.get<HitTracker>(effect);
    const auto& targetHealth = ecs.get<Health>(target);
    if (tracker.m_hitInterval > Fixed{} || (hitRow->hitCounts != -1 && tracker.m_hitCount >= hitRow->hitCounts))
        return false;
    if (targetHealth.isDead() || !targetHealth.m_beHitEnabled)
        return false;
    if (hitRow->hitInterval == -1)
    {
        if (std::find(tracker.m_hitTargets.begin(), tracker.m_hitTargets.end(), target) != tracker.m_hitTargets.end())
            return false;
        if (hitRow->hitCounts != -1 && static_cast<int32_t>(tracker.m_hitTargets.size()) >= hitRow->hitCounts)
            return false;
    }

    ctx.m_contact      = contact;
    ctx.m_effect       = effect;
    ctx.m_target       = target;
    ctx.m_attacker     = effectInfo.m_owner;
    ctx.m_effectId     = effectInfo.m_configId;
    ctx.m_skillId      = effectInfo.m_skillId;
    ctx.m_baseSkillId  = effectInfo.m_baseSkillId;
    ctx.m_slot         = effectInfo.m_slot;
    ctx.m_hit          = hitRow;
    ctx.m_effectRow    = effectRow;
    return true;
}

// 步骤 2：记一次命中。hitInterval 为 -1 时按目标去重。
void recordHit(World& world, const HitContext& ctx)
{
    auto& tracker = world.ecs().get<HitTracker>(ctx.m_effect);
    if (ctx.m_hit->hitInterval == -1)
        tracker.m_hitTargets.push_back(ctx.m_target);
    ++tracker.m_hitCount;
    if (ctx.m_hit->hitInterval != -1)
        tracker.m_hitInterval = Fixed::fromInt(ctx.m_hit->hitInterval);
}

// 步骤 5：非必中才抽样 Dodge。成功发出 Miss。
bool resolveDodge(World& world, const HitContext& ctx)
{
    if (ctx.m_hit->hitMust != 0)
        return false;
    DamageInput input;
    input.m_attacker = &world.ecs().get<Stats>(ctx.m_attacker);
    input.m_target   = &world.ecs().get<Stats>(ctx.m_target);
    input.m_hit      = ctx.m_hit;
    if (!attributeDodge(input, rng::draw(world, rng::DrawKind::Dodge)))
        return false;
    HitEvent event;
    event.m_effect   = ctx.m_effect;
    event.m_target   = ctx.m_target;
    event.m_hitDefId = ctx.m_hit->id;
    event.m_effectId = ctx.m_effectId;
    event.m_result   = HitResult::Miss;
    world.ecs().singleton<EventQueue<HitEvent>>().push(event);
    return true;
}

// 步骤 8：先暴击再浮动。
void computeDamage(World& world, HitContext& ctx)
{
    DamageInput input;
    input.m_attacker     = &world.ecs().get<Stats>(ctx.m_attacker);
    input.m_target       = &world.ecs().get<Stats>(ctx.m_target);
    input.m_hit          = ctx.m_hit;
    input.m_standardHurt = standardHurt(world, ctx.m_attacker);
    const Fixed critDraw  = rng::draw(world, rng::DrawKind::Crit);
    const Fixed floatDraw = rng::draw(world, rng::DrawKind::DamageFloat);
    ctx.m_damage          = calculateDamage(input, critDraw, floatDraw, ctx.m_critical);
}

// 步骤 9：扣血即时生效。S2: trigger BeforeDeath（护盾、锁血、免死在 health::rules 写入前）。
void applyDamage(World& world, const HitContext& ctx)
{
    health::rules::applyDelta(world, ctx.m_target, -ctx.m_damage, HpCause::Hit, ctx.m_attacker, ctx.m_effect);
}

// 步骤 10：特效与攻击者覆盖写 Pause（R18），受击者经 ScheduledActions 延迟写入（R13）。
void applyHitStop(World& world, const HitContext& ctx)
{
    const Fixed freeze = Fixed::fromInt(ctx.m_hit->freezeTime);
    auto& ecs          = world.ecs();
    if (ctx.m_hit->freezeTimeControlEffect == 0)
    {
        ecs.get<Pause>(ctx.m_effect).m_freezeMs = freeze;
        emitHitStop(world, ctx.m_effect, freeze);
    }
    if (ctx.m_hit->freezeTimeControlRole == 0)
    {
        ecs.get<Pause>(ctx.m_attacker).m_freezeMs = freeze;
        emitHitStop(world, ctx.m_attacker, freeze);
    }
    if (ecs.has<Pause>(ctx.m_target))
    {
        auto& actions = ecs.singleton<ScheduledActions>();
        actions.cancelOwner(ctx.m_target);
        actions.add(ctx.m_target, Fixed::fromInt(ctx.m_hit->freezeTimeDelay), freeze);
    }
}

// 步骤 10：写受击请求。状态转移留给下一帧 FSM。死亡与霸体不进本函数。
void requestHitReaction(World& world, const HitContext& ctx)
{
    auto& ecs              = world.ecs();
    const auto& roleState  = ecs.get<RoleState>(ctx.m_target);
    const bool inHitFamily = role::isHitState(roleState.m_state);
    const bool dead        = health::rules::isDead(ecs.get<Health>(ctx.m_target));
    const auto* roleRow =
        conf::Config::instance().find<conf::battle::RoleDef>(ecs.get<Identity>(ctx.m_target).m_configId);

    auto& hitState              = ecs.get<HitState>(ctx.m_target);
    const auto& effectTransform = ecs.get<Transform>(ctx.m_effect);
    const int32_t facing        = -effectTransform.m_towardX;
    ecs.get<Transform>(ctx.m_target).m_towardX = facing;
    ecs.get<Transform>(ctx.m_target).m_vectorX = Fixed::fromInt(facing);
    Fixed stiff                                = Fixed::fromInt(ctx.m_hit->stiffTime);
    if (roleRow && roleRow->hitStiffTime != -1)
        stiff = Fixed::fromInt(roleRow->hitStiffTime);
    hitState.m_stiffTime = stiff;
    int32_t displacementId = -1;
    if (roleRow && roleRow->hitDisplacementId != 0)
        displacementId = (roleRow->hitDisplacementId == -1 || ctx.m_hit->hitType == 0) ? ctx.m_hit->displacementId
                                                                                        : roleRow->hitDisplacementId;
    hitState.m_pendingHitDefId       = ctx.m_hit->id;
    hitState.m_pendingHitType        = ctx.m_hit->hitType;
    hitState.m_pendingDisplacementId = displacementId;
    hitState.m_pendingTowardX        = facing;
    if (!inHitFamily && !dead && ctx.m_hit->hitType != -1)
        skill::rules::forceInterrupt(world, ctx.m_target);
}

// 步骤 14：同目标已有同 id 时由 spawnHitEffect 复用。
void spawnHitEffects(World& world, const HitContext& ctx)
{
    for (const int32_t id : ctx.m_effectRow->hitEffectIds)
        if (id > 0)
            effect::rules::spawnHitEffect(world, ctx.m_attacker, id, ctx.m_target,
                                          {ctx.m_skillId, ctx.m_baseSkillId, ctx.m_slot, 0, 0});
}

// 步骤 15：命中结果。音效 Cue 留给 S2。
void emitHit(World& world, const HitContext& ctx)
{
    const auto& targetHealth = world.ecs().get<Health>(ctx.m_target);
    HitEvent hitEvent;
    hitEvent.m_attacker = ctx.m_attacker;
    hitEvent.m_effect   = ctx.m_effect;
    hitEvent.m_target   = ctx.m_target;
    hitEvent.m_hitDefId = ctx.m_hit->id;
    hitEvent.m_effectId = ctx.m_effectId;
    hitEvent.m_result   = HitResult::Hit;
    hitEvent.m_damage   = ctx.m_damage;
    hitEvent.m_critical = ctx.m_critical;
    hitEvent.m_killed   = targetHealth.m_hp <= Fixed{};
    hitEvent.m_contact  = {ctx.m_contact.m_point.x, {}, ctx.m_contact.m_point.y};
    world.ecs().singleton<EventQueue<HitEvent>>().push(hitEvent);
}
}  // namespace mugen::sim::combat::rules
