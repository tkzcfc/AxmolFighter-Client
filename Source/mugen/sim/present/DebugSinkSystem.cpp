// [11.2] DebugSink
// 读：全部事实队列
// 写：外部 `TraceSink` / 效果摘要
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 DebugSink：11.2 行，见 DESIGN.md §3.2。
void runDebugSink(World&)
{
}
}  // namespace mugen::sim
