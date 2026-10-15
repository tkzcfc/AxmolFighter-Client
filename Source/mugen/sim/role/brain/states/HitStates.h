#pragma once

#include "mugen/core/bt/BTInstance.h"
#include "mugen/sim/bt/HitActionState.h"
#include "mugen/sim/role/brain/RoleAction.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::role::brain::states
{
// 受击与角色生命周期动作叶。
struct HitState
{
    using Param = behavior::ActionType;
    using State = behavior::HitActionState;
    static void enter(RoleContext&, Param, State&);
    static BTStatus update(RoleContext&, Param, State&);
    static void exit(RoleContext&, Param, State&);
    static bool onDisplacement(RoleContext&, Param, State&);
    // 动画系统在完成真实动作时通知受击叶子。
    static void animationComplete(World&, Entity);
};
}  // namespace mugen::sim::role::brain::states
