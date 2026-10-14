#pragma once

#include "RoleBrain.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim::role::brain
{
// 进入第一个可进入的分支；无可进入则退回待机并记录一次错误。
bool enterFirst(RoleBrain& brain, World& world, Entity entity, bool city);
// 推进一帧；NotEntered 时先进入再更新（房间切换首帧语义）。
void update(RoleBrain& brain, World& world, Entity entity, bool city);
// 退出当前分支（先叶子后守卫）。
void exitBranch(RoleBrain& brain, World& world, Entity entity);
}  // namespace mugen::sim::role::brain
