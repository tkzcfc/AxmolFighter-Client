// [0.1] ClockSystem
// 读：`FrameInput` 系统命令（暂停、慢放）、`SlowMotion`
// 写：`WorldClock`、`SlowMotion`、`RoomState.m_paused`；清空 `PresentationEvents`
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
    ++clock.m_frame;
    const bool koActive = slow.m_ko.m_active;
    if (koActive)
    {
        slow.m_ko.update(kLogicDtSec);
        slow.m_scale = slow.m_ko.m_scale;
    }
    const Fixed scale = koActive || slow.m_remainingFrames != 0 ? slow.m_scale : Fixed::fromInt(1);
    if (slow.m_remainingFrames != 0)
        --slow.m_remainingFrames;
    clock.m_dtSec = kLogicDtSec * scale;
    clock.m_dtMs  = kLogicDtMs * scale;
    clock.m_elapsedMs += clock.m_dtMs;
    world.ecs().singleton<PresentationEvents>().m_events.clear();
}
}  // namespace mugen::sim
