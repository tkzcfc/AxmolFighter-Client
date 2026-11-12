// [9.1] DeathSystem
// 读：`DeathRequest`、`HealthComponent`、`IdentityComponent`、`RoleStateComponent`、`HitStateComponent`、`TransformComponent`、`StatusFlagsComponent`
// 写：`RoleStateComponent`(死亡请求位)、`DespawningComponent`
// 消费：`DeathRequest`
// 产生：`DeathEvent`、`SpawnRequest`(ko/死亡特效)、`DespawnRequest`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 发出死亡事实并生成 ko 特效。同一实体只宣布一次。
void announceDeath(World& world, Entity entity, RoleStateComponent& state)
{
    if (state.m_deathAnnounced)
        return;
    state.m_deathAnnounced = true;
    DeathEvent deathEvent;
    deathEvent.m_entity = entity;
    deathEvent.m_killer = state.m_deathKiller;
    world.ecs().singleton<EventQueue<DeathEvent>>().push(deathEvent);
    const Entity owner = world.ecs().get<IdentityComponent>(entity).m_owner;
    if (owner.value != 0 && world.ecs().alive(owner))
        buff::rules::notifySummonDeath(world, owner);
}
}  // namespace

// 系统 DeathSystem：9.1 行，见 DESIGN.md §3.2。
// 消费死亡请求并写请求位。非受击族当帧宣布死亡；受击族等状态机进入 Death 后再宣布。
// 死亡动画与死亡位移都结束后生成死亡特效并回收。
void runDeath(World& world)
{
    auto& requests = world.ecs().singleton<EventQueue<DeathRequest>>().m_items;
    for (const DeathRequest& request : requests)
    {
        if (!world.ecs().alive(request.m_entity) || !world.ecs().has<RoleStateComponent>(request.m_entity))
            continue;
        auto& state = world.ecs().get<RoleStateComponent>(request.m_entity);
        if (state.m_deathRequested)
            continue;
        const bool superArmor = world.ecs().has<StatusFlagsComponent>(request.m_entity) &&
                                world.ecs().get<StatusFlagsComponent>(request.m_entity).active(mugen::ExtraState::SuperArmor);
        const bool grounded = world.ecs().get<TransformComponent>(request.m_entity).m_position.y == Fixed{};
        // 霸体且未落地：只保护，不进入死亡。
        if (superArmor && !grounded && !role::isHitState(state.m_state))
            continue;
        state.m_deathRequested = true;
        state.m_deathKiller    = request.m_killer;
        // KO 特效在生命归零当帧生成；死亡状态再等 FSM 进入 Death 后宣布（受击族延迟到硬直/倒地结束）。
        const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(
            world.ecs().get<IdentityComponent>(request.m_entity).m_configId);
        if (role && role->koEffectId > 0)
            effect::rules::spawnFromRole(world, request.m_entity, role->koEffectId, {});
    }
    requests.clear();

    world.ecs().each<RoleStateComponent, HitStateComponent, IdentityComponent, MotionStateComponent>(
        [&](Entity entity, RoleStateComponent& state, const HitStateComponent& hit, const IdentityComponent& identity, const MotionStateComponent& motion) {
            if (world.ecs().has<DespawningComponent>(entity))
                return;
            // 非受击族在请求当帧宣布；受击族要等 FSM 写入 Death 之后。
            if (state.m_deathRequested && !state.m_deathAnnounced &&
                (state.m_state == RoleStateId::Death || !role::isHitState(state.m_state)))
                announceDeath(world, entity, state);
            if (state.m_state != RoleStateId::Death)
                return;
            const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
            const int32_t deathDisplacement = role ? role->deathDisplacementId : -1;
            const bool noDisplacement       = deathDisplacement == -1 || deathDisplacement == 0;
            const bool deathBody            = motion.m_motionName == role::motionName(RoleStateId::Death);
            if (noDisplacement || (deathBody && hit.m_animationEnd && hit.m_displacementEnd))
            {
                if (role && role->deathEffectId > 0)
                    effect::rules::spawnFromRole(world, entity, role->deathEffectId, {});
                world.ecs().add(entity, DespawningComponent{});
                world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, DespawnReason::DeathEnd});
            }
        });
}
}  // namespace mugen::sim
