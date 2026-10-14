#pragma once

#include "mugen/core/bt/BTInstance.h"
#include "mugen/sim/ai/ActionMove.h"
#include "mugen/sim/role/brain/RoleAction.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::role::brain::states
{
// 巡逻、追击和警戒共用的移动叶。
struct AiMove
{
    using Param = behavior::ActionType;
    using State = ai::ActionMove;
    static void enter(RoleContext&, Param, State&);
    static BTStatus update(RoleContext&, Param, State&);
    static void exit(RoleContext&, Param, State&);
    static bool onDisplacement(RoleContext&, Param, State&);
};
}  // namespace mugen::sim::role::brain::states
