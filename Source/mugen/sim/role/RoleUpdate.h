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
// 角色逻辑的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class RoleActor
{
public:
    // 实体组回调，须保持 void(World&, Entity) 签名以注册到 WorldHooks。
    static void update(World& world, Entity entity);
    // 为角色及特效共用动画推进与碰撞框采样。
    static void updateMotion(World& world, Entity entity);
    // 将局部伤害框按角色变换转换为世界碰撞框。
    static WorldBox transformBox(const DamageBox& box, const Transform& transform);
    // 构造指向指定世界实例中某个实体的视图。
    RoleActor(World& world, Entity entity);
    // 以固定顺序推进一个非受控角色的逻辑。
    void update();
    // 推进该实体的动画时间并重新登记碰撞框。
    void updateMotion();

private:
    World& m_world;
    Entity m_entity{};
};
}  // namespace role
}  // namespace mugen::sim
