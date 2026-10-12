#include "LogicTimers.h"
#include "../World.h"
#include "../buff/BuffPresentation.h"
#include <algorithm>

namespace mugen::sim
{
// 注册拥有者定时器；负间隔记录错误并拒绝注册。
uint64_t LogicTimers::add(Entity owner, Fixed intervalMs, bool loop, TimerPayload payload)
{
    const bool validInterval = intervalMs >= Fixed{};
    if (!validInterval)
    {
        MG_LOG_E("Timer interval must be nonnegative");
        MG_ASSERT(validInterval && "Timer interval must be nonnegative");
        return 0;
    }
    const uint64_t id = nextId++;
    entries.push_back({id, owner, {}, intervalMs, loop, std::move(payload)});
    return id;
}
// 移除与指定标识相符的活动定时器。
void LogicTimers::cancel(uint64_t id)
{
    std::erase_if(entries, [id](const auto& timer) { return timer.id == id; });
}
// 在拥有者离场时移除其全部活动定时器。
void LogicTimers::cancelOwner(Entity owner)
{
    std::erase_if(entries, [owner](const auto& timer) { return timer.owner == owner; });
}
// 重置活动定时器的累计时间，不改变其标识或载荷。
void LogicTimers::resetTimes()
{
    for (auto& timer : entries)
        timer.elapsedMs = {};
}
// 以逻辑帧步长推进定时器，并安全处理派发时的集合修改。
void updateLogicTimers(World& world)
{
    auto& timers = world.ecs().singleton<LogicTimers>();
    // IDs remain stable if dispatch mutates the timer pool; newly registered timers wait.
    std::vector<uint64_t> ids;
    for (const auto& timer : timers.entries)
        ids.push_back(timer.id);
    for (uint64_t id : ids)
    {
        auto found = std::find_if(timers.entries.begin(), timers.entries.end(),
                                  [id](const auto& timer) { return timer.id == id; });
        if (found == timers.entries.end())
            continue;
        if (std::holds_alternative<RoomWaveReady>(found->payload) &&
            (world.ecs().singleton<RoomState>().m_reviving || world.ecs().singleton<RoomState>().paused))
            continue;
        found->elapsedMs += kLogicDtMs;
        if (found->elapsedMs < found->intervalMs)
            continue;
        const LogicTimer firing = *found;
        if (found->loop)
            found->elapsedMs -= found->intervalMs;
        else
            timers.entries.erase(found);
        switch (firing.payload.index())
        {
        case 0:
            world.emit(firing.owner, std::get<EmitPresentation>(firing.payload).payload);
            break;
        case 1:
            world.destroy(std::get<DestroyEntity>(firing.payload).target);
            break;
        case 2:
            if (auto* roleTimers = world.ecs().tryGet<RoleTimers>(firing.owner))
            {
                roleTimers->freezeMs = std::get<HitFreeze>(firing.payload).m_duration;
                world.ecs().get<RoleHitState>(firing.owner).m_freezeTimer = 0;
            }
            break;
        case 3:
            buff::finishExit(world, firing.owner, std::get<BuffExit>(firing.payload).m_serial);
            break;
        case 4:
            buff::continuePresentation(world, firing.owner, std::get<BuffContinue>(firing.payload).m_serial);
            break;
        case 5:
            if (world.ecs().singleton<RoomState>().roomId == std::get<RoomWaveReady>(firing.payload).m_roomId)
                world.ecs().singleton<RoomState>().m_waveReady = true;
            break;
        }
    }
}
}  // namespace mugen::sim
