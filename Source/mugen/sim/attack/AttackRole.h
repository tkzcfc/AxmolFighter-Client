#pragma once

#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::attack
{
// 动作攻击叶。
struct AttackAction
{
    using Param = behavior::AttackAction;
    using State = AttackRole;
    static void enter(role::brain::RoleContext&, Param, State&);
    static BTStatus update(role::brain::RoleContext&, Param, State&);
    static void exit(role::brain::RoleContext&, Param, State&);
    static bool onDisplacement(role::brain::RoleContext&, Param, State&);
};
// 将动画循环完成通知转发给所有运行中的攻击叶子。
void animationComplete(World&, Entity, int64_t loops);
}  // namespace mugen::sim::attack
