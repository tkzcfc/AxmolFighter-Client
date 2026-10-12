#pragma once
#include "AttackState.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim::attack
{
/// 根据动画循环次数标记动作动画结束。
void completeAnimation(ActionAttack&, const conf::ActionConfig&, int64_t loops);
/// 在动画结束后的延迟到期时结束动作。
void dealWithTime(ActionAttack&, const conf::ActionConfig&);
/// 按动作控制配置更新移动方向和速度。
void dealWithControl(World&, Entity, const conf::ActionConfig&);
/// 将当前控制象限写入角色面向与移动向量。
void dealWithDirection(World&, Entity, bool facing);
/// 到达配置帧时仅触发一次镜头震动。
void dealWithShake(World&, Entity, ActionAttack&, const conf::ActionConfig&);
/// 处理位移碰撞事件并返回是否已被消费。
bool displacementEvent(World&, Entity, ActionAttack&, const conf::ActionConfig&, DisplacementEvent);
/// 回收本动作生成且由其拥有的效果。
void clearEffects(World&, Entity, ActionAttack&);
}
