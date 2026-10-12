#pragma once

namespace mugen::sim
{
// 碰撞系统所推进的世界实例。
class World;
namespace collision_system
{
// 扫描碰撞体并维护接触开始、持续和结束事件。
void update(World& world);
}
}
