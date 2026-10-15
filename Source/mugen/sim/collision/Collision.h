#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include <vector>

namespace mugen::sim
{
// 碰撞扫描所指向的世界实例。
class World;
// 碰撞扫描的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class CollisionSystem
{
public:
    // 世界阶段回调，须保持 void(World&) 签名以注册到 WorldHooks。
    static void update(World& world);
    // 构造指向指定世界实例的碰撞视图。
    explicit CollisionSystem(World& world);
    // 扫描碰撞体并维护接触开始、持续和结束事件。
    void update();

private:
    // 判断双方阵营掩码是否互相允许碰撞。
    static bool _inHostileMasks(const RoleIdentity& first, const RoleIdentity& second);
    // 在屏幕平面计算两个轴对齐框的交叠中心。
    static bool _intersects(const WorldBox& first, const WorldBox& second, FixedVec2& point);
    // 查找攻击框与受击框之间的首个平面接触点。
    static bool _isContacting(const ColliderState& first, const ColliderState& second, FixedVec2& point);
    // 按实体组将接触状态变化分派给对应处理器。
    void _dispatch(Entity collider, Entity other, CollisionEvent event, FixedVec2 point);
    // 为本帧未延续的历史接触补发结束事件。
    void _endMissing(Entity collider, CollisionState& state, const std::vector<CollisionContact>& active);

    // 本场战斗世界。
    World& m_world;
};
}  // namespace mugen::sim
