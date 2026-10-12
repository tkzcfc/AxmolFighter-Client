#pragma once
#include "mugen/sim/bt/RoleTree.h"

namespace mugen::sim::ai::movement
{
// 初始化 ActionPatrol、ActionChase 或 ActionAlert 的公共移动状态。
void enter(behavior::BTCtx& context, const behavior::Param& param, behavior::LeafState& state);
// 推进移动与停留，巡逻额外每帧检查目标。
BTStatus tick(behavior::BTCtx& context, const behavior::Param& param, behavior::LeafState& state);
// 退出叶子并通知动作生命周期结束。
void exit(behavior::BTCtx& context, const behavior::Param& param, behavior::LeafState& state);
// 四侧边界触发到达，不阻止位移系统钳制位置。
bool onDisplacementEvent(behavior::BTCtx& context, const behavior::Param& param, behavior::LeafState& state);
}
