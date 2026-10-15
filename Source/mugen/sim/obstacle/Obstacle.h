#pragma once
#include "mugen/sim/role/RoleComponents.h"

namespace mugen::sim
{
// 障碍物碰撞所处的世界实例。
class World;
namespace obstacle
{
// 障碍物实体的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class ObstacleActor
{
public:
    // 构造指向指定世界实例中某个障碍物实体的视图。
    ObstacleActor(World& world, Entity entity);
    // 处理角色与障碍物的碰撞状态变化。
    void onCollision(Entity character, CollisionEvent event, FixedVec2 contact);

private:
    // 本场战斗世界。
    World& m_world;
    // 障碍物实体。
    Entity m_entity{};
};
}  // namespace obstacle
}  // namespace mugen::sim
