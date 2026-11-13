// [8.3] InteractSystem
// 读：`ContactEvent`(物品)、`PortalStateComponent`、`HitboxesComponent`、`TransformComponent`、`RoomState`
// 写：`GoodsStateComponent`、`PortalStateComponent`、`BuffListComponent`(拾取)、`RoomState`(传送)
// 消费：`ContactEvent`(物品)
// 产生：`SpawnRequest`(召唤)、`RoomEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/room/RoomComponents.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim
{
namespace
{
// 推进掉落物抛射：重力积分并在落地时转入待机。
void updateSpurt(World& world, Entity entity, GoodsStateComponent& state)
{
    auto& position          = world.ecs().get<TransformComponent>(entity).m_position;
    const Fixed dt          = world.ecs().singleton<WorldClock>().m_dtMs;
    const auto step         = displacement::integrate({}, state.m_velocity.y, {}, {}, {}, dt, conf::kGravity);
    position.x += state.m_velocity.x * dt;
    position.z += state.m_velocity.z * dt * conf::kZRate;
    position.y += step.m_offset;
    state.m_velocity.y = step.m_velocity;
    if (position.y <= Fixed{} && step.m_velocity < Fixed{})
    {
        position.y = {};
        if (step.m_velocity < -conf::kBounces)
            state.m_velocity.y = -step.m_velocity * "0.2"_fx;
        else
        {
            state.m_phase    = GoodsPhase::Idle;
            state.m_velocity = {};
        }
    }
}
// 屏幕平面两轴对齐框是否交叠（与 CollisionSystem::intersects 一致）。
bool boxIntersects(const WorldBox& first, const WorldBox& second)
{
    const Fixed left   = std::max(first.m_pos.x, second.m_pos.x);
    const Fixed right  = std::min(first.m_pos.x + first.m_size.x, second.m_pos.x + second.m_size.x);
    const Fixed bottom = std::max(first.m_pos.z, second.m_pos.z);
    const Fixed top    = std::min(first.m_pos.z + first.m_size.z, second.m_pos.z + second.m_size.z);
    return left < right && bottom < top;
}
}  // namespace

// 系统 InteractSystem：8.3 行，见 DESIGN.md §3.2。
void runInteract(World& world)
{
    // 推进抛射中的掉落物。
    world.ecs().each<GoodsStateComponent, TransformComponent>(
        [&](Entity entity, GoodsStateComponent& state, const TransformComponent&) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (state.m_phase == GoodsPhase::Spurt)
                updateSpurt(world, entity, state);
        });

    // 物品拾取走 ContactEvent。
    auto& contacts = world.ecs().singleton<EventQueue<ContactEvent>>().m_items;
    for (const ContactEvent& contact : contacts)
    {
        if (contact.m_phase == ContactPhase::End)
            continue;
        const bool aGoods = world.ecs().alive(contact.m_a) && world.ecs().has<GoodsStateComponent>(contact.m_a);
        const bool bGoods = world.ecs().alive(contact.m_b) && world.ecs().has<GoodsStateComponent>(contact.m_b);
        if (!(aGoods != bGoods))
            continue;
        const Entity goods = aGoods ? contact.m_a : contact.m_b;
        const Entity role  = aGoods ? contact.m_b : contact.m_a;
        auto& state        = world.ecs().get<GoodsStateComponent>(goods);
        if (state.m_phase == GoodsPhase::Idle && world.ecs().has<RoleStateComponent>(role) &&
            !world.ecs().has<DespawningComponent>(goods))
        {
            const auto* row = conf::Config::instance().find<conf::GoodsConfig>(state.m_configId);
            if (!row)
                continue;
            state.m_phase     = GoodsPhase::Collect;
            state.m_collector = role;
            if (row->buffId != -1)
                buff::rules::admit(world, role, row->buffId);
            if (row->summonId > 0)
            {
                RoleSpawnParams params;
                params.m_roleId   = row->summonId;
                params.m_camp     = world.ecs().get<IdentityComponent>(role).m_camp;
                params.m_level    = world.ecs().get<IdentityComponent>(role).m_level;
                params.m_owner    = role;
                params.m_isAi     = true;
                params.m_position = world.ecs().get<TransformComponent>(goods).m_position;
                params.m_toward   = world.ecs().get<TransformComponent>(role).m_towardX;
                role::rules::spawnRole(world, params);
            }
            // 拾取后直接回收（省略收集动作，见报告）。
            world.ecs().add(goods, DespawningComponent{});
            world.ecs().singleton<EventQueue<DespawnRequest>>().push({goods, DespawnReason::ClipEnd});
        }
    }

    // 传送门触发：传送门与角色阵营掩码互不敌对，ContactEvent 不覆盖这一对，
    // 改由本系统按半径盒与玩家受击盒求交（对齐旧实现 CollisionSystem 的 interaction 分支）。
    auto& room = world.ecs().singleton<RoomState>();
    if (!room.m_victory)
        return;
    world.ecs().each<PortalStateComponent, TransformComponent, HitboxesComponent>(
        [&](Entity portal, PortalStateComponent& state, const TransformComponent& portalTransform,
            const HitboxesComponent& portalHitboxes) {
            if (world.ecs().has<DespawningComponent>(portal) || world.ecs().has<InactiveComponent>(portal))
                return;
            if (state.m_phase != PortalPhase::Active)
                return;
            // 传送门受击盒：以当前位置为中心、半径为 m_radius 的平面盒（对齐旧实现 _writeRadiusDamage）。
            const Fixed radius     = portalHitboxes.m_radius;
            const WorldBox portalBox{{portalTransform.m_position.x - radius, {},
                                      portalTransform.m_position.z - radius},
                                     {radius * Fixed::fromInt(2), {}, radius * Fixed::fromInt(2)}};
            // 仅英雄或受控玩家接触传送门，且需存活。
            world.ecs().each<IdentityComponent, RoleStateComponent, TransformComponent, HitboxesComponent>(
                [&](Entity role, const IdentityComponent& identity, const RoleStateComponent& roleState,
                    const TransformComponent& roleTransform, const HitboxesComponent& roleHitboxes) {
                    if (world.ecs().has<DespawningComponent>(role) || world.ecs().has<InactiveComponent>(role))
                        return;
                    if (identity.m_roleType != 1 && !identity.m_playerSlot.has_value())
                        return;
                    if (roleState.m_state == RoleStateId::Death)
                        return;
                    const Fixed deltaZ = roleTransform.m_position.z >= portalTransform.m_position.z
                                             ? roleTransform.m_position.z - portalTransform.m_position.z
                                             : portalTransform.m_position.z - roleTransform.m_position.z;
                    if (deltaZ >= portalHitboxes.m_radius + roleHitboxes.m_radius)
                        return;
                    for (const WorldBox& body : roleHitboxes.m_damage)
                        if (boxIntersects(portalBox, body))
                        {
                            // 只切阶段，换房请求由 RoomTransferSystem 下一帧读取（对齐旧实现延迟一帧）。
                            state.m_phase = PortalPhase::Transfer;
                            return;
                        }
                });
        });
}
}  // namespace mugen::sim
