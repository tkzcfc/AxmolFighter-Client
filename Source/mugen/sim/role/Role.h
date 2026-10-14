#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/sim/role/RoleComponents.h"

namespace mugen::sim
{
class World;
// 轻量角色视图：包装世界实例与实体句柄，替代原先按 (World&, Entity) 传参的命中与状态自由函数。
// 仅作为临时对象使用，不存入组件、不拥有资源、生命周期限于单次调用。
class Role
{
public:
    // 构造指向指定世界实例中某个实体的视图。
    Role(World& world, Entity entity);
    // 返回视图对应的实体句柄。
    Entity getEntity() const;
    // 读取角色状态组件。
    RoleStatus& getStatus() const;
    // 读取生命与资源组件。
    Vitals& getVitals() const;
    // 读取受击与击退组件。
    RoleHitState& getHitState() const;
    // 读取角色身份组件。
    RoleIdentity& getIdentity() const;
    // 按生命或机关剩余命中次数判断死亡。
    bool isDead() const;
    // 修改生命、同步派发免死事件，并处理死亡入口。
    void modifyHp(Fixed delta, Entity effect = {}, bool noShield = false);
    // 按状态机规则应用转换，死亡角色拒绝后续转换。
    bool changeStatus(EntityRoleStatus status, int32_t id = 0, FixedVec3 movePos = {});
    // 扣除浮空值并按连击重量加速下落。
    bool applyWeight(Fixed weight, bool hitProtect = false);
    // 查询角色当前是否触发浮空保护。
    bool isRigidity() const;
    // 读取并扣减硬直。
    Fixed getStiffTime();
    // 初始化仅在竞技模式生效的生命保护。
    void initHitProtect();
    // 追击时刷新保护清理倒计时。
    void refreshHitProtect();
    // 清除本轮保护状态，下一次命中可重新建立阈值。
    void resetHitProtect();
    // 按逻辑毫秒推进保护并检测生命阈值。
    void updateHitProtect();

private:
    // 护盾吸收后返回仍需作用于生命的负数变化。
    Fixed _absorbShield(Fixed delta);
    // 清空护盾后同步通知其所有者。
    void _cleanShield();
    World& m_world;
    Entity m_entity;
};
}  // namespace mugen::sim
