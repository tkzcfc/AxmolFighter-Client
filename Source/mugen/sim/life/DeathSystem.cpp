// [9.1] DeathSystem
// 读：`DeathRequest`、`HealthComponent`、`IdentityComponent`、`RoleStateComponent`、`HitStateComponent`、`TransformComponent`、`StatusFlagsComponent`、`TransformLinkComponent`
// 写：`RoleStateComponent`(死亡请求位)、`DespawningComponent`
// 消费：`DeathRequest`
// 产生：`DeathEvent`、`SpawnRequest`(ko/死亡特效)、`DespawnRequest`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 发出死亡事实并生成 ko 特效。同一实体只宣布一次。
void announceDeath(World& world, Entity entity, RoleStateComponent& state)
{
    if (state.deathAnnounced)
        return;
    state.deathAnnounced = true;
    DeathEvent deathEvent;
    deathEvent.entity = entity;
    deathEvent.killer = state.deathKiller;
    world.ecs().singleton<EventQueue<DeathEvent>>().push(deathEvent);
    const Entity owner = world.ecs().get<IdentityComponent>(entity).owner;
    if (owner.value != 0 && world.ecs().alive(owner))
        buff::rules::notifySummonDeath(world, owner);
}
// 采纳死亡请求：置位死亡请求、同步双生变换成员并生成生命归零当帧的 KO 特效。
void acceptDeathRequest(World& world, Entity entity, Entity killer)
{
    auto& state          = world.ecs().get<RoleStateComponent>(entity);
    state.deathRequested = true;
    state.deathKiller    = killer;
    if (world.ecs().has<TransformLinkComponent>(entity))
    {
        const auto members = world.ecs().get<TransformLinkComponent>(entity).members;
        for (const Entity member : members)
        {
            if (member.value == entity.value || !world.ecs().alive(member) ||
                world.ecs().has<DespawningComponent>(member) || !world.ecs().has<RoleStateComponent>(member))
                continue;
            auto& other = world.ecs().get<RoleStateComponent>(member);
            if (other.deathRequested)
                continue;
            other.deathRequested = true;
            other.deathKiller    = killer;
            if (world.ecs().has<HealthComponent>(member))
                world.ecs().get<HealthComponent>(member).dead = true;
        }
    }
    // KO 特效在生命归零当帧生成；死亡状态再等 FSM 进入 Death 后宣布（受击族延迟到硬直/倒地结束）。
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(
        world.ecs().get<IdentityComponent>(entity).configId);
    if (role && role->koEffectId > 0)
        effect::rules::spawnFromRole(world, entity, role->koEffectId, {});
}
}  // namespace

// 系统 DeathSystem：9.1 行，见 DESIGN.md §3.2。
// 消费死亡请求并写请求位。非受击族当帧宣布死亡；受击族等状态机进入 Death 后再宣布。
// 非玩家槽位在死亡动作结束、位移结束（或无位移）且超过死亡延迟后生成死亡特效并回收。
void runDeath(World& world)
{
    auto& requests = world.ecs().singleton<EventQueue<DeathRequest>>().items;
    for (const DeathRequest& request : requests)
    {
        if (!world.ecs().alive(request.entity) || !world.ecs().has<RoleStateComponent>(request.entity))
            continue;
        auto& state = world.ecs().get<RoleStateComponent>(request.entity);
        if (state.deathRequested)
            continue;
        const bool superArmor = world.ecs().has<StatusFlagsComponent>(request.entity) &&
                                world.ecs().get<StatusFlagsComponent>(request.entity).active(mugen::ExtraState::SuperArmor);
        const bool grounded = world.ecs().get<TransformComponent>(request.entity).position.y == Fixed{};
        // 霸体且未落地：暂缓进入死亡，先记住击杀者，落地或脱离霸体后由下方 HealthComponent::dead 扫描接管。
        if (superArmor && !grounded && !role::isHitState(state.state))
        {
            state.deathKiller = request.killer;
            if (world.ecs().has<HealthComponent>(request.entity))
                world.ecs().get<HealthComponent>(request.entity).dead = true;
            continue;
        }
        acceptDeathRequest(world, request.entity, request.killer);
    }
    requests.clear();

    world.ecs().each<HealthComponent, RoleStateComponent>(
        [&](Entity entity, const HealthComponent& health, RoleStateComponent& state) {
            if (!health.dead || state.deathRequested || world.ecs().has<DespawningComponent>(entity) ||
                world.ecs().has<InactiveComponent>(entity))
                return;
            const bool superArmor = world.ecs().has<StatusFlagsComponent>(entity) &&
                                    world.ecs().get<StatusFlagsComponent>(entity).active(mugen::ExtraState::SuperArmor);
            const bool grounded = world.ecs().get<TransformComponent>(entity).position.y == Fixed{};
            if (superArmor && !grounded && !role::isHitState(state.state))
                return;
            acceptDeathRequest(world, entity, state.deathKiller);
        });

    world.ecs().each<RoleStateComponent, HitStateComponent, IdentityComponent, MotionStateComponent>(
        [&](Entity entity, RoleStateComponent& state, const HitStateComponent& hit, const IdentityComponent& identity, const MotionStateComponent& motion) {
            if (world.ecs().has<DespawningComponent>(entity))
                return;
            // 非受击族在请求当帧宣布；受击族要等 FSM 写入 Death 之后。
            if (state.deathRequested && !state.deathAnnounced &&
                (state.state == RoleStateId::Death || !role::isHitState(state.state)))
                announceDeath(world, entity, state);
            if (state.state != RoleStateId::Death)
                return;
            // 玩家槽位实体（英雄）死亡后不回收：留在 Death 状态交给房间导演做复活/失败判定
            // （对齐旧实现 roleType != 1 时不结束死亡体；旧实现先销毁英雄再判定会误判 victory，此处不照抄）。
            if (identity.playerSlot.has_value())
                return;
            const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
            // 实际套用的死亡位移由 FSM 按死亡路径写入（倒地/受击切换死亡为 -1，不套死亡位移）。
            const int32_t deathDisplacement = hit.displacementId;
            const bool noDisplacement       = deathDisplacement == -1 || deathDisplacement == 0;
            const bool deathBody            = motion.motionName == role::motionName(RoleStateId::Death);
            if (deathBody && hit.animationEnd && (noDisplacement || hit.displacementEnd) &&
                state.stateMs > Fixed::fromInt(conf::kRoleDeathTime))
            {
                if (role && role->deathEffectId > 0)
                    effect::rules::spawnFromRole(world, entity, role->deathEffectId, {});
                room::rules::drop(world, entity);
                world.ecs().add(entity, DespawningComponent{});
                world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, DespawnReason::DeathEnd});
            }
        });
}
}  // namespace mugen::sim
