#pragma once

#include "mugen/sim/role/brain/RoleBrain.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::role::brain::states
{
// 攻击分支：按当前技能与管线生成动作序列并逐个推进攻击叶。
struct CastState
{
    // 组装施法计划并进入第一个动作；无法施法时不开始管线。
    static bool enter(RoleBrain& brain, World& world, Entity entity);
    // 推进当前攻击叶；成功则切下一个动作，序列结束或失败则结束本次施法。
    static StateStep update(RoleBrain& brain, World& world, Entity entity);
    // 退出顺序：攻击叶、管线结束、重置当前技能。
    static void exit(RoleBrain& brain, World& world, Entity entity);
    // 把位移事件交给当前攻击叶。
    static bool onDisplacement(RoleContext& ctx, CastPlan& plan);
};
}  // namespace mugen::sim::role::brain::states
