// [8.1] CombatSystem
// 读：`ContactEvent`、`EffectInfo`、`HitTracker`、`HitDef`、`Stats`、`Health`、`Identity`、`Transform`、`RoleState`
// 写：`HitTracker`、`Health`、`Pause`、`HitState`(受击请求)、`RoleState`、`Displacement`
// 消费：`ContactEvent`(特效→角色)
// 产生：`HitEvent`、`HpEvent`、`StopEvent`、`SpawnRequest`(受击特效)、`DeathRequest`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/combat/DamageRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim
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
// 顿帧帧数 = ceil(freezeMs/dtMs)。
int32_t stopFrames(Fixed freezeMs, Fixed dtMs)
{
    return static_cast<int32_t>((freezeMs.raw + dtMs.raw - 1) / dtMs.raw);
}
}  // namespace

// 系统 CombatSystem：8.1 行，见 DESIGN.md §3.2。逐个接触执行命中微管线。
void runCombat(World& world)
{
    auto& contacts = world.ecs().singleton<EventQueue<ContactEvent>>().m_items;
    const Fixed dtMs = world.ecs().singleton<WorldClock>().m_dtMs;
    for (const ContactEvent& contact : contacts)
    {
        if (contact.m_phase == ContactPhase::End)
            continue;
        const Entity effect = contact.m_a;
        const Entity target = contact.m_b;
        auto& ecs           = world.ecs();
        if (!ecs.alive(effect) || ecs.has<Despawning>(effect) || !ecs.has<EffectInfo>(effect))
            continue;
        if (!ecs.alive(target) || ecs.has<Despawning>(target) || !ecs.has<Health>(target))
            continue;
        const auto& effectInfo = ecs.get<EffectInfo>(effect);
        const auto* effectRow  = conf::Config::instance().find<conf::battle::EffectDef>(effectInfo.m_configId);
        if (!effectRow || effectRow->hitId <= 0)
            continue;
        const auto* hitRow = conf::Config::instance().find<conf::battle::HitDef>(effectRow->hitId);
        if (!hitRow)
            continue;
        if (!ecs.alive(effectInfo.m_owner) || !ecs.has<Stats>(effectInfo.m_owner))
            continue;
        // 目标过滤：特效命中敌对阵营，友方不产生打击。
        const auto& ownerIdentity = ecs.get<Identity>(effectInfo.m_owner);
        const auto& targetIdentity = ecs.get<Identity>(target);
        if ((static_cast<uint32_t>(ownerIdentity.m_hostileMask) & static_cast<uint32_t>(targetIdentity.m_camp)) !=
            static_cast<uint32_t>(targetIdentity.m_camp))
            continue;

        auto& tracker = ecs.get<HitTracker>(effect);
        auto& targetHealth = ecs.get<Health>(target);
        // 门控：命中间隔 / 次数 / 目标去重 / 死亡 / 可受击。
        if (tracker.m_hitInterval > Fixed{} || (hitRow->hitCounts != -1 && tracker.m_hitCount >= hitRow->hitCounts))
            continue;
        if (targetHealth.isDead() || !targetHealth.m_beHitEnabled)
            continue;
        if (hitRow->hitInterval == -1)
        {
            if (std::find(tracker.m_hitTargets.begin(), tracker.m_hitTargets.end(), target) != tracker.m_hitTargets.end())
                continue;
            if (hitRow->hitCounts != -1 &&
                static_cast<int32_t>(tracker.m_hitTargets.size()) >= hitRow->hitCounts)
                continue;
            tracker.m_hitTargets.push_back(target);
        }
        ++tracker.m_hitCount;
        if (hitRow->hitInterval != -1)
            tracker.m_hitInterval = Fixed::fromInt(hitRow->hitInterval);

        // 属性闪避（必中跳过；DrawPolicy 下固定不闪避）。
        combat::rules::DamageInput input;
        input.m_attacker     = &ecs.get<Stats>(effectInfo.m_owner);
        input.m_target       = &ecs.get<Stats>(target);
        input.m_hit          = hitRow;
        input.m_standardHurt = standardHurt(world, effectInfo.m_owner);
        const Fixed dodgeDraw = world.drawPolicy() ? world.drawPolicy()->m_dodge : Fixed::fromInt(100);
        if (combat::rules::attributeDodge(input, dodgeDraw))
        {
            HitEvent event;
            event.m_effect   = effect;
            event.m_target   = target;
            event.m_hitDefId = hitRow->id;
            event.m_effectId = effectInfo.m_configId;
            event.m_result   = HitResult::Miss;
            ecs.singleton<EventQueue<HitEvent>>().push(event);
            continue;
        }

        // 伤害公式（R15：暴击再浮动）。
        bool critical       = false;
        const Fixed critDraw = world.drawPolicy() ? world.drawPolicy()->m_crit : Fixed::fromInt(101);
        const Fixed floatDraw = world.drawPolicy() ? world.drawPolicy()->m_float : Fixed::fromInt(50);
        const Fixed damage = combat::rules::calculateDamage(input, critDraw, floatDraw, critical);

        // 扣血（即时生效）。
        targetHealth.m_hp = std::clamp(targetHealth.m_hp - damage, Fixed{}, targetHealth.m_maxHp);
        HpEvent hpEvent;
        hpEvent.m_entity = target;
        hpEvent.m_delta  = -damage;
        hpEvent.m_cause  = HpCause::Hit;
        hpEvent.m_source = effectInfo.m_owner;
        ecs.singleton<EventQueue<HpEvent>>().push(hpEvent);

        // 霸体（受击保护）：只做受击保护，跳过顿帧、朝向、硬直与受击状态转移（旧实现 _hitReaction）。
        const bool superArmor = ecs.has<StatusFlags>(target) &&
                                ecs.get<StatusFlags>(target).active(mugen::ExtraState::SuperArmor);
        auto& roleState           = ecs.get<RoleState>(target);
        const bool inHitFamily    = role::isHitState(roleState.m_state);
        const auto* roleRow = conf::Config::instance().find<conf::battle::RoleDef>(ecs.get<Identity>(target).m_configId);
        if (!superArmor)
        {
            // 顿帧（覆盖写：特效 / 攻击者当帧，受击者延迟）。
            const Fixed freeze = Fixed::fromInt(hitRow->freezeTime);
            if (hitRow->freezeTimeControlEffect == 0)
            {
                if (ecs.has<EffectInfo>(effect))
                    ecs.get<EffectInfo>(effect).m_freezeMs = freeze;
                ecs.singleton<EventQueue<StopEvent>>().push({effect, stopFrames(freeze, dtMs)});
            }
            if (hitRow->freezeTimeControlRole == 0)
            {
                ecs.get<Pause>(effectInfo.m_owner).m_freezeMs = freeze;
                ecs.singleton<EventQueue<StopEvent>>().push({effectInfo.m_owner, stopFrames(freeze, dtMs)});
            }
            if (ecs.has<Pause>(target))
            {
                auto& actions = ecs.singleton<ScheduledActions>();
                actions.cancelOwner(target);
                actions.add(target, Fixed::fromInt(hitRow->freezeTimeDelay), freeze);
            }

            // 受击反应：朝向、硬直、受击请求。
            auto& hitState                 = ecs.get<HitState>(target);
            const auto& effectTransform    = ecs.get<Transform>(effect);
            const int32_t facing           = -effectTransform.m_towardX;
            ecs.get<Transform>(target).m_towardX = facing;
            ecs.get<Transform>(target).m_vectorX = Fixed::fromInt(facing);
            Fixed stiff                    = Fixed::fromInt(hitRow->stiffTime);
            if (roleRow && roleRow->hitStiffTime != -1)
                stiff = Fixed::fromInt(roleRow->hitStiffTime);
            hitState.m_stiffTime = stiff;
            // 选择受击位移：hitType 0 用命中配置，否则用角色覆盖（-1 时用命中配置）。
            int32_t displacementId = -1;
            if (roleRow && roleRow->hitDisplacementId != 0)
                displacementId = (roleRow->hitDisplacementId == -1 || hitRow->hitType == 0)
                                     ? hitRow->displacementId
                                     : roleRow->hitDisplacementId;
            hitState.m_pendingHitDefId       = hitRow->id;
            hitState.m_pendingHitType        = hitRow->hitType;
            hitState.m_pendingDisplacementId = displacementId;
            hitState.m_pendingTowardX        = facing;
            // 击杀：立即进入死亡。
            if (targetHealth.m_hp <= Fixed{} && !inHitFamily)
            {
                roleState.m_state = RoleStateId::Death;
                StateEvent stateEvent;
                stateEvent.m_entity = target;
                stateEvent.m_from   = static_cast<int32_t>(roleState.m_prev);
                stateEvent.m_to     = static_cast<int32_t>(roleState.m_state);
                ecs.singleton<EventQueue<StateEvent>>().push(stateEvent);
                DeathEvent deathEvent;
                deathEvent.m_entity = target;
                deathEvent.m_killer = effectInfo.m_owner;
                ecs.singleton<EventQueue<DeathEvent>>().push(deathEvent);
                if (roleRow && roleRow->koEffectId > 0)
                    effect::rules::spawnFromRole(world, target, roleRow->koEffectId, {});
            }
            else if (!inHitFamily && hitRow->hitType != -1)
            {
                roleState.m_state = RoleStateId::Hit;
                StateEvent stateEvent;
                stateEvent.m_entity = target;
                stateEvent.m_from   = static_cast<int32_t>(roleState.m_prev);
                stateEvent.m_to     = static_cast<int32_t>(roleState.m_state);
                ecs.singleton<EventQueue<StateEvent>>().push(stateEvent);
            }
        }
        else if (targetHealth.m_hp <= Fixed{} && !inHitFamily &&
                 ecs.get<Transform>(target).m_position.y == Fixed{})
        {
            // 霸体下仅落地且已死时进入死亡；否则只刷新受击保护。
            roleState.m_state = RoleStateId::Death;
            StateEvent stateEvent;
            stateEvent.m_entity = target;
            stateEvent.m_from   = static_cast<int32_t>(roleState.m_prev);
            stateEvent.m_to     = static_cast<int32_t>(roleState.m_state);
            ecs.singleton<EventQueue<StateEvent>>().push(stateEvent);
            DeathEvent deathEvent;
            deathEvent.m_entity = target;
            deathEvent.m_killer = effectInfo.m_owner;
            ecs.singleton<EventQueue<DeathEvent>>().push(deathEvent);
            if (roleRow && roleRow->koEffectId > 0)
                effect::rules::spawnFromRole(world, target, roleRow->koEffectId, {});
        }

        // 命中结果事件。
        HitEvent hitEvent;
        hitEvent.m_attacker = effectInfo.m_owner;
        hitEvent.m_effect   = effect;
        hitEvent.m_target   = target;
        hitEvent.m_hitDefId = hitRow->id;
        hitEvent.m_effectId = effectInfo.m_configId;
        hitEvent.m_result   = HitResult::Hit;
        hitEvent.m_damage   = damage;
        hitEvent.m_critical = critical;
        hitEvent.m_killed   = targetHealth.m_hp <= Fixed{};
        hitEvent.m_contact  = {contact.m_point.x, {}, contact.m_point.y};
        ecs.singleton<EventQueue<HitEvent>>().push(hitEvent);

        // 受击特效。
        for (const int32_t id : effectRow->hitEffectIds)
            if (id > 0)
                effect::rules::spawnHitEffect(world, effectInfo.m_owner, id, target,
                                              {effectInfo.m_skillId, effectInfo.m_baseSkillId, effectInfo.m_slot, 0, 0});
    }
}
}  // namespace mugen::sim
