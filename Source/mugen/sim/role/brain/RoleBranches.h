#pragma once

#include "RoleBrain.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::role::brain
{
// 普通模板的分支数量。
inline constexpr size_t kBranchCount = 19;
// 城镇模板的分支数量（Finding, Idle）。
inline constexpr size_t kTownBranchCount = 2;
// 分支对应的动作类型（用于叶子分发）。
behavior::ActionType branchAction(RoleBranch branch);
// 分支守卫是否成立（复制自 RoleTree.cpp check 的 Guard 逻辑）。
bool guardHolds(RoleBranch branch, World& world, Entity entity, bool city);
// 退出守卫的副作用（复制自 RoleTree.cpp condExit 的 Guard 逻辑）。
void exitGuard(RoleBranch branch, World& world, Entity entity);
}  // namespace mugen::sim::role::brain
