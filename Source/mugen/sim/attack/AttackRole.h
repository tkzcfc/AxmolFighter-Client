#pragma once

#include "mugen/core/bt/BTInstance.h"
#include "mugen/sim/attack/AttackState.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::attack
{
// 攻击叶进入参数：动作配置、列内序号与技能下标。
struct AttackLeafParam
{
    // 动作配置 id。
    int32_t m_actionId = -1;
    // 同方向动作列表中的序号，从 1 起。
    int32_t m_actionIndex = 1;
    // 来源技能数组下标。
    int32_t m_skill = -1;
};
// 动作攻击叶。
struct AttackAction
{
    using Param = AttackLeafParam;
    using State = AttackRole;
    // 初始化攻击动作并应用开始帧副作用。
    static void enter(role::brain::RoleContext&, Param, State&);
    // 推进攻击动作时序，完成时返回成功。
    static BTStatus update(role::brain::RoleContext&, Param, State&);
    // 撤销攻击动作施加的临时资源。
    static void exit(role::brain::RoleContext&, Param, State&);
    // 处理当前攻击的位移事件。
    static bool onDisplacement(role::brain::RoleContext&, Param, State&);
    // 把动画循环完成通知转给当前施法中的攻击叶。
    static void animationComplete(World&, Entity, int64_t loops);
};
}  // namespace mugen::sim::attack
