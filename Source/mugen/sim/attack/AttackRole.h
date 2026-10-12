#pragma once
#include "mugen/sim/bt/RoleTree.h"

namespace mugen::sim::attack
{
/// 初始化一次动作攻击叶子并启动动作表现。
void enter(behavior::BTCtx&, const behavior::Param&, behavior::LeafState&);
/// 推进动作帧事件、打断窗口和结束条件。
BTStatus tick(behavior::BTCtx&, const behavior::Param&, behavior::LeafState&);
/// 清理动作创建的状态、效果和临时 Buff。
void exit(behavior::BTCtx&, const behavior::Param&, behavior::LeafState&);
/// 将位移系统事件交给当前动作处理。
bool onDisplacementEvent(behavior::BTCtx&, const behavior::Param&, behavior::LeafState&);
/// 将动画循环完成通知转发给所有运行中的攻击叶子。
void animationComplete(World&, Entity, int64_t loops);
}
