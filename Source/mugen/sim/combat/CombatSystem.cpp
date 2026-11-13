// [8.1] CombatSystem
// 读：`ContactEvent`、`EffectInfoComponent`、`HitTrackerComponent`、`HitDef`、`StatsComponent`、`StatusFlagsComponent`、`RoleStateComponent`、`SkillCastComponent`、`HealthComponent`、`IdentityComponent`、`TransformComponent`
// 写：`HitTrackerComponent`、`HealthComponent`（经 health::rules）、`PauseComponent`、`HitStateComponent`(受击请求)、`SkillCastComponent`(强制打断)、`TransformComponent`(受击朝向)
// 消费：`ContactEvent`(特效→角色)
// 产生：`HitEvent`、`HpEvent`、`DeathRequest`、`HitStopEvent`、`SpawnRequest`(受击特效)、`DespawnRequest`、`CastEvent`(Cancelled)；RNG
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/combat/CombatRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/conf/battle/HitDef.h"
#include <algorithm>
#include <vector>

namespace mugen::sim
{
namespace
{
// 实体创建序（each 的遍历序），供 R5 排序。
size_t creationRank(const std::vector<Entity>& created, Entity entity)
{
    const auto it = std::find(created.begin(), created.end(), entity);
    return it == created.end() ? created.size() : static_cast<size_t>(it - created.begin());
}
}  // namespace

// 系统 CombatSystem：8.1 行，见 DESIGN.md §6.6。只收集并按 R5 排序接触，逐个走完命中微管线。
void runCombat(World& world)
{
    std::vector<Entity> created;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent&) { created.push_back(entity); });
    auto contacts = world.ecs().singleton<EventQueue<ContactEvent>>().m_items;
    std::stable_sort(contacts.begin(), contacts.end(), [&](const ContactEvent& a, const ContactEvent& b) {
        const size_t attacker = creationRank(created, a.m_a);
        const size_t other    = creationRank(created, b.m_a);
        if (attacker != other)
            return attacker < other;
        return creationRank(created, a.m_b) < creationRank(created, b.m_b);
    });

    std::vector<Entity> hitEffects;
    for (const ContactEvent& contact : contacts)
    {
        if (contact.m_phase == ContactPhase::End)
            continue;
        combat::rules::HitContext ctx;
        if (!combat::rules::gate(world, contact, ctx))
            continue;
        combat::rules::recordHit(world, ctx);
        hitEffects.push_back(ctx.m_effect);
        BuffContext hitCtx;
        hitCtx.m_applicator = ctx.m_attacker;
        hitCtx.m_target     = ctx.m_target;
        hitCtx.m_skillId    = ctx.m_baseSkillId > 0 ? ctx.m_baseSkillId : ctx.m_skillId;
        hitCtx.m_boundSkill = hitCtx.m_skillId;
        hitCtx.m_effectId   = ctx.m_effectId;
        const bool invincible = world.ecs().has<StatusFlagsComponent>(ctx.m_target) &&
                                world.ecs().get<StatusFlagsComponent>(ctx.m_target).active(mugen::ExtraState::Invincible);
        const bool invincibleGate = invincible && ctx.m_effectRow && !ctx.m_effectRow->hitExtraControl.empty() &&
                                    ctx.m_effectRow->hitExtraControl.front() == 0 && ctx.m_hit->hitMust != 1;
        if (invincibleGate)
        {
            const bool dodging = skill::rules::isDodging(world, ctx.m_target);
            HitEvent event;
            event.m_effect   = ctx.m_effect;
            event.m_target   = ctx.m_target;
            event.m_attacker = ctx.m_attacker;
            event.m_hitDefId = ctx.m_hit->id;
            event.m_effectId = ctx.m_effectId;
            event.m_result   = dodging ? HitResult::Evade : HitResult::NoDamage;
            world.ecs().singleton<EventQueue<HitEvent>>().push(event);
            if (dodging)
                buff::rules::fire(world, ctx.m_target, conf::battle::TriggerPoint::EvadeSuccess, hitCtx);
            continue;
        }
        if (skill::rules::isDodging(world, ctx.m_target))
        {
            HitEvent event;
            event.m_effect   = ctx.m_effect;
            event.m_target   = ctx.m_target;
            event.m_attacker = ctx.m_attacker;
            event.m_hitDefId = ctx.m_hit->id;
            event.m_effectId = ctx.m_effectId;
            event.m_result   = HitResult::Evade;
            world.ecs().singleton<EventQueue<HitEvent>>().push(event);
            buff::rules::fire(world, ctx.m_target, conf::battle::TriggerPoint::EvadeSuccess, hitCtx);
            continue;
        }
        if (combat::rules::resolveDodge(world, ctx))
        {
            buff::rules::fire(world, ctx.m_target, conf::battle::TriggerPoint::AttackMiss, hitCtx);
            continue;
        }
        buff::rules::fire(world, ctx.m_target, conf::battle::TriggerPoint::BeforeToBeHit, hitCtx);
        // hitType == -1：无伤害也无硬直。原版 dealWithHitX 已记命中次数并触发 BEFORE_TOBEHIT，
        // 但跳过 BeforeHit / 伤害 / 顿帧 / 受击状态 / 怒气 / AfterToBeHit / AfterHit，也不发 HitEvent。
        // 仍施加 debuff 并播受击特效（hitEffectIds 为空时无特效生成）。
        if (ctx.m_hit->hitType == -1)
        {
            if (ctx.m_effectRow)
                for (const int32_t id : ctx.m_effectRow->debuffId)
                    if (id > 0)
                        buff::rules::admit(world, ctx.m_target, id, hitCtx);
            combat::rules::spawnHitEffects(world, ctx);
            continue;
        }
        buff::rules::fire(world, ctx.m_attacker, conf::battle::TriggerPoint::BeforeHit, hitCtx);
        combat::rules::computeDamage(world, ctx);
        combat::rules::applyDamage(world, ctx);
        hitCtx.m_damage   = ctx.m_damage;
        hitCtx.m_critical = ctx.m_critical;
        const bool superArmor = world.ecs().has<StatusFlagsComponent>(ctx.m_target) &&
                                world.ecs().get<StatusFlagsComponent>(ctx.m_target).active(mugen::ExtraState::SuperArmor);
        if (!superArmor)
        {
            combat::rules::applyHitStop(world, ctx);
            combat::rules::requestHitReaction(world, ctx);
        }
        skill::rules::grantPartnerRageOnHit(world, ctx.m_attacker);
        const bool dead = world.ecs().has<HealthComponent>(ctx.m_target) && world.ecs().get<HealthComponent>(ctx.m_target).m_dead;
        hitCtx.m_targetDead = dead;
        if (!dead)
            buff::rules::fire(world, ctx.m_target, conf::battle::TriggerPoint::AfterToBeHit, hitCtx);
        buff::rules::fire(world, ctx.m_attacker, conf::battle::TriggerPoint::AfterHit, hitCtx);
        if (!skill::rules::isDodging(world, ctx.m_target) && ctx.m_effectRow)
            for (const int32_t id : ctx.m_effectRow->debuffId)
                if (id > 0)
                    buff::rules::admit(world, ctx.m_target, id, hitCtx);
        combat::rules::emitHit(world, ctx);
        combat::rules::spawnHitEffects(world, ctx);
    }
    // 帧末统一提交命中间隔（旧实现同帧多目标都能命中）。
    for (const Entity effect : hitEffects)
        combat::rules::commitHitInterval(world, effect);
}
}  // namespace mugen::sim
