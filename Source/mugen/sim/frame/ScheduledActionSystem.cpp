// [0.2] ScheduledActionSystem
// 读：`ScheduledActions`、`WorldClock`
// 写：`PauseComponent`（受击者顿帧）
// 消费：`ScheduledActions`
// 产生：`HitStopEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 ScheduledActionSystem：0.2 行，见 DESIGN.md §3.2。推进延迟动作（受击顿帧）。
void runScheduledAction(World& world)
{
    auto& actions = world.ecs().singleton<ScheduledActions>();
    const Fixed dt = world.ecs().singleton<WorldClock>().dtMs;
    for (size_t i = 0; i < actions.actions.size();)
    {
        auto& action = actions.actions[i];
        action.dueMs -= dt;
        if (action.dueMs > Fixed{})
        {
            ++i;
            continue;
        }
        // 到期：写受击者顿帧并发出 stop 行。
        if (world.ecs().alive(action.owner) && world.ecs().has<PauseComponent>(action.owner))
        {
            world.ecs().get<PauseComponent>(action.owner).freezeMs = action.freezeMs;
            HitStopEvent event;
            event.entity   = action.owner;
            event.freezeMs = action.freezeMs;
            world.ecs().singleton<EventQueue<HitStopEvent>>().push(event);
        }
        actions.actions.erase(actions.actions.begin() + static_cast<std::ptrdiff_t>(i));
    }
}
}  // namespace mugen::sim
