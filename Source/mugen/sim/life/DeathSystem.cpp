// [9.1] DeathSystem
// 读：`DeathRequest`、`Health`、`Identity`、`RoleState`、`HitState`、`Transform`、`StatusFlags`
// 写：`RoleState`(死亡请求位)、`Despawning`
// 消费：`DeathRequest`
// 产生：`DeathEvent`、`SpawnRequest`(ko/死亡特效)、`DespawnRequest`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 发出死亡事实并生成 ko 特效。同一实体只宣布一次。
void announceDeath(World& world, Entity entity, RoleState& state)
{
    if (state.m_deathAnnounced)
        return;
    state.m_deathAnnounced = true;
    DeathEvent deathEvent;
    deathEvent.m_entity = entity;
    deathEvent.m_killer = state.m_deathKiller;
    world.ecs().singleton<EventQueue<DeathEvent>>().push(deathEvent);
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(
        world.ecs().get<Identity>(entity).m_configId);
    if (role && role->koEffectId > 0)
        effect::rules::spawnFromRole(world, entity, role->koEffectId, {});
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
        if (!world.ecs().alive(request.m_entity) || !world.ecs().has<RoleState>(request.m_entity))
            continue;
        auto& state = world.ecs().get<RoleState>(request.m_entity);
        if (state.m_deathRequested)
            continue;
        const bool superArmor = world.ecs().has<StatusFlags>(request.m_entity) &&
                                world.ecs().get<StatusFlags>(request.m_entity).active(mugen::ExtraState::SuperArmor);
        const bool grounded = world.ecs().get<Transform>(request.m_entity).m_position.y == Fixed{};
        // 霸体且未落地：只保护，不进入死亡。
        if (superArmor && !grounded && !role::isHitState(state.m_state))
            continue;
        state.m_deathRequested = true;
        state.m_deathKiller    = request.m_killer;
    }
    requests.clear();

    world.ecs().each<RoleState, HitState, Identity, MotionState>(
        [&](Entity entity, RoleState& state, const HitState& hit, const Identity& identity, const MotionState& motion) {
            if (world.ecs().has<Despawning>(entity))
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
                world.ecs().add(entity, Despawning{});
                world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, DespawnReason::DeathEnd});
            }
        });
}
}  // namespace mugen::sim
