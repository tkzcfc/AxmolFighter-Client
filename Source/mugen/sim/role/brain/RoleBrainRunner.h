#pragma once

#include "RoleBrain.h"

namespace mugen::sim::role::brain
{
// 进入第一个可进入的分支；无可进入返回 false（调用方置 Stopped）。
bool enterFirst(RoleBrain& brain, World& world, Entity entity, bool city);
// 推进一帧；NotEntered 时先进入再更新（房间切换首帧语义）。
void update(RoleBrain& brain, World& world, Entity entity, bool city);
// 消费本帧控制命令并写入 RoleControl.m_last。
void consumeControl(World& world, Entity entity);
// 退出当前分支（先叶子后守卫）。
void exitBranch(RoleBrain& brain, World& world, Entity entity);
// 分派位移事件（角色侧：当前分支或遗留受击监听）。
bool onDisplacementEvent(World& world, Entity entity, DisplacementEvent event);
}  // namespace mugen::sim::role::brain
