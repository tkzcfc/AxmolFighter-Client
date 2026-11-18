// [8.1] CombatSystem
// 读：`ContactEvent`、`EffectInfoComponent`、`HitTrackerComponent`、`HitDef`、`StatsComponent`、`StatusFlagsComponent`、`RoleStateComponent`、`SkillCastComponent`、`HealthComponent`、`IdentityComponent`、`TransformComponent`、`ComboMeterComponent`、`SummonComponent`
// 写：`HitTrackerComponent`、`HealthComponent`（经 health::rules）、`PauseComponent`、`HitStateComponent`(受击请求)、`SkillCastComponent`(强制打断)、`TransformComponent`(受击朝向)、`ComboMeterComponent`、`AiBrainComponent`(仇恨)
// 消费：`ContactEvent`(特效→角色)
// 产生：`HitEvent`、`HpEvent`、`DeathRequest`、`HitStopEvent`、`SpawnRequest`(受击特效)、`DespawnRequest`、`CastEvent`(Cancelled)；RNG
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/combat/CombatRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/BattleConst.h"
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
// 首次命中记录当前生命减去最大生命比例后的阈值。
void initHitProtect(World& world, Entity target)
{
    if (!world.setup().pvp || !world.ecs().has<HitStateComponent>(target))
        return;
    auto& hitState = world.ecs().get<HitStateComponent>(target);
    if (hitState.hitProtectHp)
        return;
    const auto& health          = world.ecs().get<HealthComponent>(target);
    hitState.hitProtectHp       = std::max(Fixed{}, health.hp - health.maxHp * conf::kHitProtectHpScale);
    hitState.hitProtectTime     = Fixed::fromInt(conf::kHitProtectCleanTime);
    hitState.hitProtectExecuted = false;
}
}  // namespace

// 系统 CombatSystem：8.1 行，见 DESIGN.md §6.6。只收集并按 R5 排序接触，逐个走完命中微管线。
void runCombat(World& world)
{
    std::vector<Entity> created;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent&) { created.push_back(entity); });
    auto contacts = world.ecs().singleton<EventQueue<ContactEvent>>().items;
    std::stable_sort(contacts.begin(), contacts.end(), [&](const ContactEvent& a, const ContactEvent& b) {
        const size_t attacker = creationRank(created, a.a);
        const size_t other    = creationRank(created, b.a);
        if (attacker != other)
            return attacker < other;
        return creationRank(created, a.b) < creationRank(created, b.b);
    });

    std::vector<Entity> hitEffects;
    for (const ContactEvent& contact : contacts)
    {
        if (contact.phase == ContactPhase::End)
            continue;
        combat::rules::HitContext ctx;
        if (!combat::rules::gate(world, contact, ctx))
            continue;
        combat::rules::recordHit(world, ctx);
        hitEffects.push_back(ctx.effect);
        BuffContext hitCtx;
        hitCtx.applicator = ctx.attacker;
        hitCtx.target     = ctx.target;
        hitCtx.skillId    = ctx.baseSkillId > 0 ? ctx.baseSkillId : ctx.skillId;
        hitCtx.boundSkill = hitCtx.skillId;
        hitCtx.effectId   = ctx.effectId;
        const bool invincible = world.ecs().has<StatusFlagsComponent>(ctx.target) &&
                                world.ecs().get<StatusFlagsComponent>(ctx.target).active(mugen::ExtraState::Invincible);
        const bool invincibleGate = invincible && ctx.effectRow && !ctx.effectRow->hitExtraControl.empty() &&
                                    ctx.effectRow->hitExtraControl.front() == 0 && ctx.hit->hitMust != 1;
        if (invincibleGate)
        {
            const bool dodging = skill::rules::isDodging(world, ctx.target);
            HitEvent event;
            event.effect   = ctx.effect;
            event.target   = ctx.target;
            event.attacker = ctx.attacker;
            event.hitDefId = ctx.hit->id;
            event.effectId = ctx.effectId;
            event.result   = dodging ? HitResult::Evade : HitResult::NoDamage;
            world.ecs().singleton<EventQueue<HitEvent>>().push(event);
            if (dodging)
                buff::rules::fire(world, ctx.target, conf::battle::TriggerPoint::EvadeSuccess, hitCtx);
            continue;
        }
        if (skill::rules::isDodging(world, ctx.target))
        {
            HitEvent event;
            event.effect   = ctx.effect;
            event.target   = ctx.target;
            event.attacker = ctx.attacker;
            event.hitDefId = ctx.hit->id;
            event.effectId = ctx.effectId;
            event.result   = HitResult::Evade;
            world.ecs().singleton<EventQueue<HitEvent>>().push(event);
            buff::rules::fire(world, ctx.target, conf::battle::TriggerPoint::EvadeSuccess, hitCtx);
            continue;
        }
        if (combat::rules::resolveDodge(world, ctx))
        {
            buff::rules::fire(world, ctx.target, conf::battle::TriggerPoint::AttackMiss, hitCtx);
            continue;
        }
        buff::rules::fire(world, ctx.target, conf::battle::TriggerPoint::BeforeToBeHit, hitCtx);
        // hitType == -1：无伤害也无硬直。原版 dealWithHitX 已记命中次数并触发 BEFORE_TOBEHIT，
        // 但跳过 BeforeHit / 伤害 / 顿帧 / 受击状态 / 怒气 / AfterToBeHit / AfterHit，也不发 HitEvent。
        // 仍施加 debuff 并播受击特效（hitEffectIds 为空时无特效生成）。
        if (ctx.hit->hitType == -1)
        {
            if (ctx.effectRow)
                for (const int32_t id : ctx.effectRow->debuffId)
                    if (id > 0)
                        buff::rules::admit(world, ctx.target, id, hitCtx);
            combat::rules::spawnHitEffects(world, ctx);
            continue;
        }
        buff::rules::fire(world, ctx.attacker, conf::battle::TriggerPoint::BeforeHit, hitCtx);
        combat::rules::computeDamage(world, ctx);
        combat::rules::applyDamage(world, ctx);
        hitCtx.damage   = ctx.damage;
        hitCtx.critical = ctx.critical;
        const bool superArmor = world.ecs().has<StatusFlagsComponent>(ctx.target) &&
                                world.ecs().get<StatusFlagsComponent>(ctx.target).active(mugen::ExtraState::SuperArmor);
        if (!superArmor)
        {
            combat::rules::applyHitStop(world, ctx);
            combat::rules::requestHitReaction(world, ctx);
        }
        else if (world.ecs().has<HealthComponent>(ctx.target) && !world.ecs().get<HealthComponent>(ctx.target).dead)
            initHitProtect(world, ctx.target);
        skill::rules::grantPartnerRageOnHit(world, ctx.attacker);
        combat::rules::settleHitFeedback(world, ctx);
        const bool dead = world.ecs().has<HealthComponent>(ctx.target) && world.ecs().get<HealthComponent>(ctx.target).dead;
        hitCtx.targetDead = dead;
        if (!dead)
            buff::rules::fire(world, ctx.target, conf::battle::TriggerPoint::AfterToBeHit, hitCtx);
        if (ctx.effectRow)
        {
            const int32_t targetRoleType =
                world.ecs().has<IdentityComponent>(ctx.target) ? world.ecs().get<IdentityComponent>(ctx.target).roleType
                                                               : 0;
            combat::rules::recoverEp(world, ctx.attacker,
                                     ctx.effectRow->energy * (targetRoleType == 4 ? "1.5"_fx : "1"_fx));
        }
        buff::rules::fire(world, ctx.attacker, conf::battle::TriggerPoint::AfterHit, hitCtx);
        if (!skill::rules::isDodging(world, ctx.target) && ctx.effectRow)
            for (const int32_t id : ctx.effectRow->debuffId)
                if (id > 0)
                    buff::rules::admit(world, ctx.target, id, hitCtx);
        combat::rules::emitHit(world, ctx);
        combat::rules::spawnHitEffects(world, ctx);
    }
    // 帧末统一提交命中间隔（旧实现同帧多目标都能命中）。
    for (const Entity effect : hitEffects)
        combat::rules::commitHitInterval(world, effect);
}
}  // namespace mugen::sim
