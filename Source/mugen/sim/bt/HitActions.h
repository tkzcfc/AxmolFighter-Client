#pragma once
#include "RoleTree.h"

namespace mugen::sim::behavior::hit_actions
{
// 进入指定受击或生命周期动作。
void enter(BTCtx&, const Param&, LeafState&);
// 推进受击计时、追击和挣脱。
BTStatus tick(BTCtx&, const Param&, LeafState&);
// 清理叶子拥有的位移。
void exit(BTCtx&, const Param&, LeafState&);
// 消费位移通知并决定是否禁止反弹。
bool onDisplacementEvent(BTCtx&, const Param&, LeafState&);
// 动画系统在完成真实动作时通知受击叶子。
void animationComplete(World&, Entity);
}
