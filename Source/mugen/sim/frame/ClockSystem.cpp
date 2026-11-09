// [0.1] ClockSystem
// 读：`FrameInput` 系统命令（暂停、慢放）、`SlowMotion`
// 写：`WorldClock`、`SlowMotion`、`RoomState.m_paused`；清空 `PresentationEvents`
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 ClockSystem：0.1 行，见 DESIGN.md §3.2。
void runClock(World& world)
{
    auto& clock = world.ecs().singleton<WorldClock>();
    ++clock.m_frame;
    clock.m_elapsedMs += clock.m_dtMs;
    world.ecs().singleton<PresentationEvents>().m_events.clear();
}
}  // namespace mugen::sim
