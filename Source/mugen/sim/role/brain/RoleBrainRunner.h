#pragma once

#include "RoleBrain.h"

namespace mugen::sim::role::brain
{
// 进入第一个可进入的分支；无可进入返回 false（调用方置 Stopped）。
bool enterFirst(RoleBrain& brain, World& world, Entity entity, bool city);
// 推进一帧；NotEntered 时先进入再更新（房间切换首帧语义）。
void update(RoleBrain& brain, World& world, Entity entity, bool city);
// 退出当前分支（先叶子后守卫）。
void exitBranch(RoleBrain& brain, World& world, Entity entity);
}  // namespace mugen::sim::role::brain
