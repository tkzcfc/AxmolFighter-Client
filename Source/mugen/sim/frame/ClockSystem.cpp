// [0.1] ClockSystem
// 读：`FrameInput` 系统命令（暂停、慢放）、`SlowMotion`
// 写：`WorldClock`、`SlowMotion`、`RoomState.paused`；清空 `PresentationEvents`
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 ClockSystem：0.1 行，见 DESIGN.md §3.2。推进帧号并缩放本帧逻辑时间。
// KO 慢放曲线用未缩放的逻辑秒推进，避免慢放反过来延长自身（参照旧实现 World::tick）。
void runClock(World& world)
{
    auto& clock = world.ecs().singleton<WorldClock>();
    auto& slow  = world.ecs().singleton<SlowMotion>();
    ++clock.frame;
    const bool koActive = slow.ko.active;
    if (koActive)
    {
        slow.ko.update(kLogicDtSec);
        slow.scale = slow.ko.scale;
    }
    const Fixed scale = koActive || slow.remainingFrames != 0 ? slow.scale : Fixed::fromInt(1);
    if (slow.remainingFrames != 0)
        --slow.remainingFrames;
    clock.dtSec = kLogicDtSec * scale;
    clock.dtMs  = kLogicDtMs * scale;
    clock.elapsedMs += clock.dtMs;
    world.ecs().singleton<PresentationEvents>().events.clear();
}
}  // namespace mugen::sim
