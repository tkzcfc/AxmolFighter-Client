#pragma once

#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::role::brain::states
{
// 待机 / 行走 / 跑动通用动作叶，按 ActionType 分发。
struct Locomotion
{
    using Param = behavior::ActionType;
    using State = behavior::ActionState;
    static void enter(RoleContext&, Param, State&);
    static BTStatus update(RoleContext&, Param, State&);
    static void exit(RoleContext&, Param, State&);
};
// 传送门推挤位移叶。
struct Jostled
{
    using Param = behavior::ActionType;
    using State = behavior::JostledState;
    static void enter(RoleContext&, Param, State&);
    static BTStatus update(RoleContext&, Param, State&);
    static void exit(RoleContext&, Param, State&);
    static bool onDisplacement(RoleContext&, Param, State&);
};
// 路径点寻路叶，按世界模式选择走跑并委托 Locomotion。
struct Finding
{
    using Param = behavior::ActionType;
    using State = behavior::ActionState;
    static void enter(RoleContext&, Param, State&);
    static BTStatus update(RoleContext&, Param, State&);
    static void exit(RoleContext&, Param, State&);
};
}  // namespace mugen::sim::role::brain::states
