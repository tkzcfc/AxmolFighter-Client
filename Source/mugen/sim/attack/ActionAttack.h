#pragma once
#include "AttackState.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim::attack
{
// 动作攻击的临时驱动视图，不存入组件、不拥有资源，生命周期限于单次调用。
class ActionAttackDriver
{
public:
    // 构造指向指定世界实例中某个实体动作攻击状态的视图。
    ActionAttackDriver(World& world, Entity entity, ActionAttack& action);
    // 按动作控制配置更新移动方向和速度。
    void dealWithControl(const conf::ActionConfig& row);
    // 将当前控制象限写入角色面向与移动向量。
    void dealWithDirection(bool facing);
    // 到达配置帧时仅触发一次镜头震动。
    void dealWithShake(const conf::ActionConfig& row);
    // 处理位移碰撞事件并返回是否已被消费。
    bool onDisplacementEvent(const conf::ActionConfig& row, DisplacementEvent event);
    // 回收本动作生成且由其拥有的效果。
    void clearEffects();

private:
    World& m_world;
    Entity m_entity{};
    ActionAttack& m_action;
};
}  // namespace mugen::sim::attack
