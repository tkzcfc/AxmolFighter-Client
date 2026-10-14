#pragma once

#include "RoleBrain.h"

namespace mugen::sim::role::brain
{
// 将控制事件转换为方向角并交给技能系统。
void skillEvent(World& world, Entity entity, const PlayerCommand& command);
// 消费本帧控制队列，更新最近命令并分派技能输入。
void consumeControl(World& world, Entity entity, RoleControl& control);
}  // namespace mugen::sim::role::brain
