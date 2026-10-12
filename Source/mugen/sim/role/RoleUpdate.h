#pragma once
#include "RoleComponents.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/DamageBox.h"
namespace mugen::sim
{
// 角色系统所更新的世界实例。
class World;
namespace role
{
// 将局部伤害框按角色变换转换为世界碰撞框。
WorldBox transformBox(const DamageBox& box, const Transform& transform);
// 以固定顺序推进一个非受控角色的逻辑。
void update(World& world, Entity entity);
// 为角色及特效共用动画推进与碰撞框采样。
void updateMotion(World& world, Entity entity);
}  // namespace role
}  // namespace mugen::sim
