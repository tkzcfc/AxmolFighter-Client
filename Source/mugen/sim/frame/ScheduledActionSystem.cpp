// [0.2] ScheduledActionSystem
// 读：`ScheduledActions`、`WorldClock`
// 写：`Pause`（延迟顿帧）、`RoomState`（波次到时）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 ScheduledActionSystem：0.2 行，见 DESIGN.md §3.2。
void runScheduledAction(World&)
{
}
}  // namespace mugen::sim
