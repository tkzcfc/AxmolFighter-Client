#include "Obstacle.h"
#include "mugen/sim/World.h"

namespace mugen::sim::obstacle
{
ObstacleActor::ObstacleActor(World& world, Entity entity) : m_world(world), m_entity(entity) {}

// 根据上一帧位置决定从横边还是纵深边推出，避免穿越障碍中心。
void ObstacleActor::onCollision(Entity character, CollisionEvent event, FixedVec2)
{
    auto& ecs = m_world.ecs();
    if (event == CollisionEvent::End || ecs.get<RoleIdentity>(m_entity).camp != 1) return;
    const auto& wall = ecs.get<ColliderState>(m_entity);
    const auto& body = ecs.get<ColliderState>(character);
    if (wall.damage.empty() || body.damage.empty()) return;
    const auto& a = body.damage.front();
    const auto& b = wall.damage.front();
    auto& position = ecs.get<Transform>(character).position;
    const auto* displacement = ecs.tryGet<Displacement>(character);
    const auto offset = displacement ? displacement->offset : FixedVec3{};
    if (position.x - offset.x <= b.pos.x)
        position.x += b.pos.x - a.pos.x - a.size.x;
    else if (position.x - offset.x >= b.pos.x + b.size.x)
        position.x += b.pos.x + b.size.x - a.pos.x;
    else
    {
        const auto z = ecs.get<Transform>(m_entity).position.z;
        position.z = z + (position.z - offset.z <= z ? -(wall.radius + body.radius) : wall.radius + body.radius);
    }
}
}  // namespace mugen::sim::obstacle
