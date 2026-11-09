#pragma once

#include "RoleBrain.h"

#include <array>
#include <cstddef>

namespace mugen::sim_legacy::role::brain
{
// 普通模板的分支数量。
inline constexpr size_t kBranchCount = 19;
// 城镇模板的分支数量。
inline constexpr size_t kTownBranchCount = 4;
// 城镇模板的分支顺序：寻路、跑动、行走、待机。可控角色按输入在城镇走跑。
inline constexpr std::array<RoleBranch, kTownBranchCount> kTownBranches = {
    RoleBranch::Finding, RoleBranch::Run, RoleBranch::Walk, RoleBranch::Idle};
}  // namespace mugen::sim_legacy::role::brain
