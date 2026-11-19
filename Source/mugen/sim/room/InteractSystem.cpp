// [8.3] InteractSystem
// 读：`ContactEvent`(物品)、`PortalStateComponent`、`HitboxesComponent`、`TransformComponent`、`RoomState`
// 写：`GoodsStateComponent`、`PortalStateComponent`、`BuffListComponent`(拾取)、`RoomState`(传送)
// 消费：`ContactEvent`(物品)
// 产生：`SpawnRequest`(召唤)、`RoomEvent`、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/anim/HitboxRules.h"
#include "mugen/sim/room/RoomComponents.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <string>

namespace mugen::sim
{
namespace
{
// 推进掉落物抛射：重力积分并在落地时转入待机。
void updateSpurt(World& world, Entity entity, GoodsStateComponent& state)
{
    auto& position          = world.ecs().get<TransformComponent>(entity).position;
    const Fixed dt          = world.ecs().singleton<WorldClock>().dtMs;
    const auto step         = displacement::integrate({}, state.velocity.y, {}, {}, {}, dt, conf::kGravity);
    position.x += state.velocity.x * dt;
    position.z += state.velocity.z * dt * conf::kZRate;
    position.y += step.offset;
    state.velocity.y = step.velocity;
    if (position.y <= Fixed{} && step.velocity < Fixed{})
    {
        position.y = {};
        if (step.velocity < -conf::kBounces)
            state.velocity.y = -step.velocity * "0.2"_fx;
        else
        {
            state.phase    = GoodsPhase::Idle;
            state.velocity = {};
        }
    }
}
// 横向、纵深、高度三轴都有正长度交集。
bool boxIntersects(const WorldBox& first, const WorldBox& second)
{
    return worldBoxesOverlap(first, second, nullptr);
}
}  // namespace

// 系统 InteractSystem：8.3 行，见 DESIGN.md §3.2。
void runInteract(World& world)
{
    if (world.setup().mode == WorldMode::Town)
        return;
    // 推进抛射中的掉落物。
    world.ecs().each<GoodsStateComponent, TransformComponent>(
        [&](Entity entity, GoodsStateComponent& state, const TransformComponent&) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (state.phase == GoodsPhase::Spurt)
                updateSpurt(world, entity, state);
        });

    // 物品拾取走 ContactEvent。
    auto& contacts = world.ecs().singleton<EventQueue<ContactEvent>>().items;
    for (const ContactEvent& contact : contacts)
    {
        if (contact.phase == ContactPhase::End)
            continue;
        const bool aGoods = world.ecs().alive(contact.a) && world.ecs().has<GoodsStateComponent>(contact.a);
        const bool bGoods = world.ecs().alive(contact.b) && world.ecs().has<GoodsStateComponent>(contact.b);
        if (!(aGoods != bGoods))
            continue;
        const Entity goods = aGoods ? contact.a : contact.b;
        const Entity role  = aGoods ? contact.b : contact.a;
        auto& state        = world.ecs().get<GoodsStateComponent>(goods);
        if (state.phase == GoodsPhase::Idle && world.ecs().has<RoleStateComponent>(role) &&
            !world.ecs().has<DespawningComponent>(goods))
        {
            const auto* row = conf::Config::instance().find<conf::GoodsConfig>(state.configId);
            if (!row)
                continue;
            state.phase     = GoodsPhase::Collect;
            state.collector = role;
            if (row->buffId != -1)
                buff::rules::admit(world, role, row->buffId);
            if (row->soundPick > 0)
            {
                CueEvent event;
                event.entity = goods;
                event.kind   = CueKind::Sound;
                event.id     = row->soundPick;
                world.ecs().singleton<EventQueue<CueEvent>>().push(event);
            }
            if (row->summonId > 0)
            {
                RoleSpawnParams params;
                params.roleId   = row->summonId;
                params.camp     = world.ecs().get<IdentityComponent>(role).camp;
                params.level    = world.ecs().get<IdentityComponent>(role).level;
                params.owner    = role;
                params.isAi     = true;
                params.position = world.ecs().get<TransformComponent>(goods).position;
                params.toward   = world.ecs().get<TransformComponent>(role).towardX;
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
    if (!room.victory)
        return;
    world.ecs().each<PortalStateComponent, TransformComponent, HitboxesComponent>(
        [&](Entity portal, PortalStateComponent& state, const TransformComponent& portalTransform,
            const HitboxesComponent& portalHitboxes) {
            if (world.ecs().has<DespawningComponent>(portal) || world.ecs().has<InactiveComponent>(portal))
                return;
            if (state.phase != PortalPhase::Active)
                return;
            // 传送门体积：半径铺横向和纵深，高度取直径。不读命中深度字段。
            const Fixed radius = portalHitboxes.radius;
            const Fixed span   = radius * Fixed::fromInt(2);
            const WorldBox portalBox{{portalTransform.position.x - radius, portalTransform.position.z - radius,
                                      portalTransform.position.y},
                                     {span, span, span}};
            // 仅英雄或受控玩家接触传送门，且需存活。
            world.ecs().each<IdentityComponent, RoleStateComponent, TransformComponent, HitboxesComponent>(
                [&](Entity role, const IdentityComponent& identity, const RoleStateComponent& roleState,
                    const TransformComponent&, const HitboxesComponent& roleHitboxes) {
                    if (world.ecs().has<DespawningComponent>(role) || world.ecs().has<InactiveComponent>(role))
                        return;
                    if (identity.roleType != 1 && !identity.playerSlot.has_value())
                        return;
                    if (roleState.state == RoleStateId::Death)
                        return;
                    for (const WorldBox& body : roleHitboxes.damage)
                        if (boxIntersects(portalBox, body))
                        {
                            // 只切阶段，换房请求由 RoomTransferSystem 下一帧读取（对齐旧实现延迟一帧）。
                            state.phase = PortalPhase::Transfer;
                            const auto* row = conf::Config::instance().find<conf::PortalConfig>(state.configId);
                            if (row && world.ecs().has<MotionStateComponent>(portal))
                            {
                                auto& motion      = world.ecs().get<MotionStateComponent>(portal);
                                motion.motionName = room::rules::portalMotion(*row, PortalPhase::Transfer);
                                motion.loop       = room::rules::portalLoop(*row, PortalPhase::Transfer, false);
                                motion.playing    = true;
                                motion.timeMs     = {};
                                int32_t id        = 0;
                                for (const char c : motion.entryId)
                                    if (c >= '0' && c <= '9')
                                        id = id * 10 + (c - '0');
                                motion.entryId = std::to_string(id + 1);
                            }
                            return;
                        }
                });
        });
}
}  // namespace mugen::sim
