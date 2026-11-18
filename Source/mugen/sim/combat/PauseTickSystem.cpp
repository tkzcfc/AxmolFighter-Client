// [2.4] PauseTickSystem
// 读：`PauseComponent`（角色与特效）、`HitTrackerComponent`、`WorldClock`
// 写：`PauseComponent`（顿帧/定身递减并计算 `paused`）、未暂停时的 `HitTrackerComponent.hitInterval`
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 PauseTickSystem：2.4 行，见 DESIGN.md §3.2。
void runPauseTick(World& world)
{
    const Fixed dt = world.ecs().singleton<WorldClock>().dtMs;
    world.ecs().each<PauseComponent>([&](Entity entity, PauseComponent& pause) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        pause.freezeMs = std::max(Fixed{}, pause.freezeMs - dt);
        pause.staticMs = std::max(Fixed{}, pause.staticMs - dt);
        pause.paused   = pause.freezeMs > Fixed{} || pause.staticMs > Fixed{};
        // 特效命中间隔随特效暂停；未暂停时递减，0 与负值保留。
        if (!pause.paused)
            if (auto* tracker = world.ecs().tryGet<HitTrackerComponent>(entity))
                if (tracker->hitInterval > Fixed{})
                    tracker->hitInterval = std::max(Fixed{}, tracker->hitInterval - dt);
    });
}
}  // namespace mugen::sim
