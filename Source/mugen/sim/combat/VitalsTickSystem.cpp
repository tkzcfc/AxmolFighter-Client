// [2.3] VitalsTickSystem
// 读：`HealthComponent`、`StatsComponent`、`RoleStateComponent`、`SkillCastComponent`、`IdentityComponent`、`RageStateComponent`、`SummonComponent`
// 写：MP 回复、暴气 EP、`MoveClock`、生命/能量采样、`SkillCastComponent.m_crazy`、伙伴怒气冻结与回退、召唤寿命
// 消费：—
// 产生：同步触发 AfterCrazy（暴气能量耗尽）、`DeathRequest`(召唤到期)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>

namespace mugen::sim
{
// 系统 VitalsTickSystem：2.3 行。MP 回复、暴气消耗、走跑/站立计时与资源采样。
void runVitalsTick(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().m_dtMs;
    const Fixed dtSec = world.ecs().singleton<WorldClock>().m_dtSec;
    world.ecs().each<HealthComponent, RoleStateComponent>([&](Entity entity, HealthComponent& health, RoleStateComponent& state) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        const Fixed scale = world.ecs().has<StatsComponent>(entity) ? world.ecs().get<StatsComponent>(entity).finalStat(203)
                                                           : Fixed::fromInt(1);
        health.m_mp = std::min(health.m_maxMp, health.m_mp + dtMs * conf::kMpIncreaseBasicValue * scale);
        // 跑动同时带站立位，状态位不会恰好等于 Run，连续移动秒数保持为 0。
        state.m_movedSec =
            role::statusBits(state.m_state) == EntityRoleStatus::Run ? state.m_movedSec + dtSec : Fixed{};
        state.m_standingSec = state.m_state == RoleStateId::Idle ? state.m_standingSec + dtSec : Fixed{};
        health.m_prevHp = health.m_hp;
        health.m_prevEp = health.m_ep;
        if (world.ecs().has<SkillCastComponent>(entity) && world.ecs().get<SkillCastComponent>(entity).m_crazy)
        {
            Fixed extra{};
            if (const auto* identity = world.ecs().tryGet<IdentityComponent>(entity))
            {
                const int32_t id = identity->m_configId;
                if (id >= 1 && id <= 4)
                {
                    const Fixed additions[] = {conf::kCrazyDecayValueRole1, conf::kCrazyDecayValueRole2,
                                               conf::kCrazyDecayValueRole3, conf::kCrazyDecayValueRole4};
                    extra                    = additions[id - 1];
                }
            }
            health.m_ep = std::max(Fixed{}, health.m_ep - dtMs * (conf::kCrazyDecayBasicValue + extra));
            if (!(health.m_ep > Fixed{}))
            {
                // 冻结实现里槽位变化默认已置位，能量归零即结束暴气。
                world.ecs().get<SkillCastComponent>(entity).m_crazy = false;
                BuffContext ctx;
                ctx.m_target = entity;
                buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterCrazy, ctx);
            }
        }
        if (!world.ecs().has<RageStateComponent>(entity) || !world.ecs().has<SkillCastComponent>(entity))
            return;
        auto& rage = world.ecs().get<RageStateComponent>(entity);
        auto& cast = world.ecs().get<SkillCastComponent>(entity);
        rage.m_rage = std::clamp(rage.m_rage, Fixed{}, conf::kPartnerRageMaxAnger);
        if (rage.m_transformed && rage.m_freezeMs > Fixed{})
        {
            rage.m_freezeMs = std::max(Fixed{}, rage.m_freezeMs - dtMs);
            if (rage.m_freezeMs == Fixed{})
                cast.m_partnerRageRecoverPending = true;
        }
        const bool canRecover = (state.m_state == RoleStateId::Idle || role::isMoveState(state.m_state)) &&
                                state.m_state != RoleStateId::Death && state.m_state != RoleStateId::Revive &&
                                state.m_state != RoleStateId::Jostled && !role::isTobeHit(state.m_state) &&
                                !state.m_deathRequested;
        if (!rage.m_transformed || !cast.m_partnerRageRecoverPending || !canRecover)
            return;
        rage.m_rage                          = {};
        rage.m_freezeMs                      = {};
        rage.m_transformed                   = false;
        cast.m_partnerRageRecoverPending     = false;
        cast.m_partnerRagePendingGain        = {};
        cast.m_partnerRagePendingGainActive  = false;
    });
    world.ecs().each<SummonComponent, HealthComponent>([&](Entity entity, SummonComponent& summon, HealthComponent& health) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        if (summon.m_remainingMs < Fixed{})
            return;
        summon.m_remainingMs -= dtMs;
        if (summon.m_remainingMs <= Fixed{})
        {
            summon.m_remainingMs = "-1"_fx;
            health::rules::applyDelta(world, entity, -health.m_hp, HpCause::Cost, entity);
        }
    });
}
}  // namespace mugen::sim
