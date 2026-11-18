// [8.2] ObstacleSystem
// 读：`ContactEvent`(特效→障碍、角色↔障碍)
// 写：`ObstacleStateComponent`、`TransformComponent`(推回)
// 消费：`ContactEvent`
// 产生：`DespawnRequest`、`SpawnRequest`(掉落特效)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 障碍物被击破：按死亡特效与掉落结算，然后回收。
void breakObstacle(World& world, Entity obstacle)
{
    auto& state     = world.ecs().get<ObstacleStateComponent>(obstacle);
    const auto* row = conf::Config::instance().find<conf::ObstacleConfig>(state.configId);
    if (row && row->deathEffectId != -1)
        effect::rules::spawnFromRole(world, obstacle, row->deathEffectId, {});
    life::rules::despawn(world, obstacle, DespawnReason::DeathEnd);
}
// 角色撞上障碍物时按上一帧位置推开，避免穿越中心。
void pushBack(World& world, Entity obstacle, Entity character)
{
    const auto& wall  = world.ecs().get<HitboxesComponent>(obstacle);
    const auto& body  = world.ecs().get<HitboxesComponent>(character);
    if (wall.damage.empty() || body.damage.empty())
        return;
    const auto& a           = body.damage.front();
    const auto& b           = wall.damage.front();
    auto& position          = world.ecs().get<TransformComponent>(character).position;
    const auto* displacement = world.ecs().tryGet<DisplacementComponent>(character);
    const FixedVec3 offset  = displacement ? displacement->offset : FixedVec3{};
    if (position.x - offset.x <= b.pos.x)
        position.x += b.pos.x - a.pos.x - a.size.x;
    else if (position.x - offset.x >= b.pos.x + b.size.x)
        position.x += b.pos.x + b.size.x - a.pos.x;
    else
    {
        const Fixed z = world.ecs().get<TransformComponent>(obstacle).position.z;
        position.z    = z + (position.z - offset.z <= z ? -(wall.radius + body.radius)
                                                        : wall.radius + body.radius);
    }
}
}  // namespace

// 系统 ObstacleSystem：8.2 行，见 DESIGN.md §3.2。
void runObstacle(World& world)
{
    auto& contacts = world.ecs().singleton<EventQueue<ContactEvent>>().items;
    for (const ContactEvent& contact : contacts)
    {
        if (contact.phase == ContactPhase::End)
            continue;
        const bool aObstacle = world.ecs().alive(contact.a) && world.ecs().has<ObstacleStateComponent>(contact.a);
        const bool bObstacle = world.ecs().alive(contact.b) && world.ecs().has<ObstacleStateComponent>(contact.b);
        if (aObstacle == bObstacle)
            continue;  // 双方都不是或都是障碍。
        const Entity obstacle  = aObstacle ? contact.a : contact.b;
        const Entity other     = aObstacle ? contact.b : contact.a;
        if (world.ecs().has<DespawningComponent>(obstacle))
            continue;
        if (world.ecs().has<EffectInfoComponent>(other))
        {
            // 特效命中障碍：扣减受击次数，耗尽后击破。
            auto& state = world.ecs().get<ObstacleStateComponent>(obstacle);
            if (state.hits > 0 && --state.hits == 0)
                breakObstacle(world, obstacle);
        }
        else if (world.ecs().has<RoleStateComponent>(other))
        {
            pushBack(world, obstacle, other);
        }
    }
}
}  // namespace mugen::sim
