// [2.3] VitalsTickSystem
// 读：`HealthComponent`、`StatsComponent`、`RoleStateComponent`、`SkillCastComponent`、`IdentityComponent`、`RageStateComponent`、`SummonComponent`、`ComboMeterComponent`、`HitStateComponent`
// 写：MP 回复、暴气 EP、`MoveClock`、生命/能量采样、`SkillCastComponent.crazy`、伙伴怒气冻结与回退、召唤寿命、`ComboMeterComponent`(超时与衰减)、`HitStateComponent`(受击保护)
// 消费：—
// 产生：同步触发 AfterCrazy（暴气能量耗尽）、`DeathRequest`(召唤到期)、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/RoleDef.h"
#include <algorithm>

namespace mugen::sim
{
namespace
{
// 是否持有指定 Buff（跳过已标记移除的实例）。
bool hasBuff(World& world, Entity entity, int32_t id)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return false;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
        if (!inst.removing && inst.id == id)
            return true;
    return false;
}
}  // namespace

// 系统 VitalsTickSystem：2.3 行。MP 回复、暴气消耗、走跑/站立计时与资源采样。
void runVitalsTick(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().dtMs;
    const Fixed dtSec = world.ecs().singleton<WorldClock>().dtSec;
    world.ecs().each<HealthComponent, RoleStateComponent>([&](Entity entity, HealthComponent& health, RoleStateComponent& state) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        const Fixed scale = world.ecs().has<StatsComponent>(entity) ? world.ecs().get<StatsComponent>(entity).finalStat(203)
                                                           : Fixed::fromInt(1);
        health.mp = std::min(health.maxMp, health.mp + dtMs * conf::kMpIncreaseBasicValue * scale);
        // 跑动同时带站立位，状态位不会恰好等于 Run，连续移动秒数保持为 0。
        state.movedSec =
            role::statusBits(state.state) == EntityRoleStatus::Run ? state.movedSec + dtSec : Fixed{};
        state.standingSec = state.state == RoleStateId::Idle ? state.standingSec + dtSec : Fixed{};
        health.prevHp = health.hp;
        health.prevEp = health.ep;
        if (world.ecs().has<SkillCastComponent>(entity) && world.ecs().get<SkillCastComponent>(entity).crazy)
        {
            Fixed extra{};
            if (const auto* identity = world.ecs().tryGet<IdentityComponent>(entity))
            {
                const int32_t id = identity->configId;
                if (id >= 1 && id <= 4)
                {
                    const Fixed additions[] = {conf::kCrazyDecayValueRole1, conf::kCrazyDecayValueRole2,
                                               conf::kCrazyDecayValueRole3, conf::kCrazyDecayValueRole4};
                    extra                    = additions[id - 1];
                }
            }
            health.ep = std::max(Fixed{}, health.ep - dtMs * (conf::kCrazyDecayBasicValue + extra));
            if (!(health.ep > Fixed{}))
            {
                // 冻结实现里槽位变化默认已置位，能量归零即结束暴气。
                world.ecs().get<SkillCastComponent>(entity).crazy = false;
                BuffContext ctx;
                ctx.target = entity;
                buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterCrazy, ctx);
            }
        }
        if (!world.ecs().has<RageStateComponent>(entity) || !world.ecs().has<SkillCastComponent>(entity))
            return;
        auto& rage = world.ecs().get<RageStateComponent>(entity);
        auto& cast = world.ecs().get<SkillCastComponent>(entity);
        rage.rage = std::clamp(rage.rage, Fixed{}, conf::kPartnerRageMaxAnger);
        if (rage.transformed && rage.freezeMs > Fixed{})
        {
            rage.freezeMs = std::max(Fixed{}, rage.freezeMs - dtMs);
            if (rage.freezeMs == Fixed{})
                cast.partnerRageRecoverPending = true;
        }
        const bool canRecover = (state.state == RoleStateId::Idle || role::isMoveState(state.state)) &&
                                state.state != RoleStateId::Death && state.state != RoleStateId::Revive &&
                                state.state != RoleStateId::Jostled && !role::isTobeHit(state.state) &&
                                !state.deathRequested;
        if (!rage.transformed || !cast.partnerRageRecoverPending || !canRecover)
            return;
        rage.rage                          = {};
        rage.freezeMs                      = {};
        rage.transformed                   = false;
        cast.partnerRageRecoverPending     = false;
        CueEvent cue;
        cue.entity = entity;
        cue.kind   = CueKind::PartnerRage;
        cue.param  = 0;
        world.ecs().singleton<EventQueue<CueEvent>>().push(cue);
        cast.partnerRagePendingGain        = {};
        cast.partnerRagePendingGainActive  = false;
    });
    // 推进受击保护：超时清除阈值，跌破阈值则强制下落并挂保护 Buff。
    world.ecs().each<HitStateComponent, HealthComponent, RoleStateComponent, IdentityComponent>(
        [&](Entity entity, HitStateComponent& hit, HealthComponent& health, const RoleStateComponent& state,
            const IdentityComponent& identity) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (world.setup().mode == WorldMode::Town && !identity.playerSlot.has_value())
                return;
            if (!hit.hitProtectHp || hit.hitProtectExecuted)
                return;
            hit.hitProtectTime -= dtMs;
            if (hit.hitProtectTime <= Fixed{})
            {
                hit.hitProtectHp.reset();
                hit.hitProtectTime     = {};
                hit.hitProtectExecuted = false;
                return;
            }
            if (health.hp > *hit.hitProtectHp)
                return;
            if (role::isHitState(state.state))
            {
                const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
                hit.rigidity     = {};
                if (!health.dead && role && role->rigidity > 0)
                {
                    world.ecs().get<DisplacementComponent>(entity).velocity.y -=
                        hit.weight * Fixed::fromInt(hit.hitCounts);
                    if (identity.roleType == 1 && !hasBuff(world, entity, 155) && !hasBuff(world, entity, 173))
                    {
                        buff::rules::admit(world, entity, 173);
                        buff::rules::admit(world, entity, 154);
                    }
                }
            }
            hit.hitProtectExecuted = true;
        });
    world.ecs().each<SummonComponent, HealthComponent>([&](Entity entity, SummonComponent& summon, HealthComponent& health) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        if (summon.remainingMs < Fixed{})
            return;
        summon.remainingMs -= dtMs;
        if (summon.remainingMs <= Fixed{})
        {
            summon.remainingMs = "-1"_fx;
            health::rules::applyDelta(world, entity, -health.hp, HpCause::Cost, entity);
        }
    });
    world.ecs().each<ComboMeterComponent>([&](Entity entity, ComboMeterComponent& meter) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        if (meter.count > 0)
        {
            meter.timerMs += dtMs;
            if (meter.timerMs >= conf::kComboTimeMs)
            {
                meter.count = 0;
                meter.timerMs = {};
                meter.killedTargets.clear();
            }
        }
        if (meter.grade > 0)
        {
            meter.grade = static_cast<int32_t>(conf::kComboStageExp.size());
            for (size_t i = 0; i < conf::kComboStageExp.size(); ++i)
            {
                if (conf::kComboStageExp[i] > meter.exp)
                {
                    meter.grade = static_cast<int32_t>(i + 1);
                    break;
                }
            }
            if (meter.exp <= Fixed{} && meter.grade <= 1)
                meter.grade = 0;
            else
                meter.exp -= conf::kComboStageSpeed[static_cast<size_t>(meter.grade - 1)];
        }
    });
}
}  // namespace mugen::sim
