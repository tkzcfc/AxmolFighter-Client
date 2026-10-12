#pragma once
#include "mugen/sim/role/RoleComponents.h"

namespace mugen::sim
{
// 障碍物碰撞所处的世界实例。
class World;
namespace obstacle
{
// 处理角色与障碍物的碰撞状态变化。
void onCollision(World& world, Entity character, Entity obstacle, CollisionEvent event, FixedVec2 contact);
}
}
