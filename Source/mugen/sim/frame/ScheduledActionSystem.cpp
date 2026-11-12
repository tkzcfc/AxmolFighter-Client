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
    const Fixed dt = world.ecs().singleton<WorldClock>().m_dtMs;
    for (size_t i = 0; i < actions.m_actions.size();)
    {
        auto& action = actions.m_actions[i];
        action.m_dueMs -= dt;
        if (action.m_dueMs > Fixed{})
        {
            ++i;
            continue;
        }
        // 到期：写受击者顿帧并发出 stop 行。
        if (world.ecs().alive(action.m_owner) && world.ecs().has<PauseComponent>(action.m_owner))
        {
            world.ecs().get<PauseComponent>(action.m_owner).m_freezeMs = action.m_freezeMs;
            HitStopEvent event;
            event.m_entity   = action.m_owner;
            event.m_freezeMs = action.m_freezeMs;
            world.ecs().singleton<EventQueue<HitStopEvent>>().push(event);
        }
        actions.m_actions.erase(actions.m_actions.begin() + static_cast<std::ptrdiff_t>(i));
    }
}
}  // namespace mugen::sim
