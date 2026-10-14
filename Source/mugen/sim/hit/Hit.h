#pragma once
#include "mugen/sim/role/RoleComponents.h"

namespace mugen::sim
{
// 命中特效所处的世界实例。
class World;
namespace hit
{
// 处理特效碰撞状态变化。
void onEffectCollision(World& world, Entity effect, Entity other, CollisionEvent event, FixedVec2 contact);
}  // namespace hit
}  // namespace mugen::sim
