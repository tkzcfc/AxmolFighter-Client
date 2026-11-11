// [2.4] PauseTickSystem
// 读：`Pause`（角色与特效）、`WorldClock`
// 写：`Pause`（顿帧/定身递减并计算 `m_paused`）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 PauseTickSystem：2.4 行，见 DESIGN.md §3.2。
void runPauseTick(World& world)
{
    const Fixed dt = world.ecs().singleton<WorldClock>().m_dtMs;
    world.ecs().each<Pause>([&](Entity entity, Pause& pause) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        pause.m_freezeMs = std::max(Fixed{}, pause.m_freezeMs - dt);
        pause.m_staticMs = std::max(Fixed{}, pause.m_staticMs - dt);
        pause.m_paused   = pause.m_freezeMs > Fixed{} || pause.m_staticMs > Fixed{};
    });
}
}  // namespace mugen::sim
