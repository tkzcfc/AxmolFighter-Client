// [12.2] FlushSystem
// 读：—
// 写：`flushDestroyed()`；清空全部请求/事实队列（Debug 断言请求队列已排空）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/core/MacroDefinition.h"

namespace mugen::sim
{
namespace
{
// 清空全部请求与事实队列（PresentationEvents 不在此清空，保留到下一帧 ClockSystem）。
void clearEventQueues(World& world)
{
    auto& ecs = world.ecs();
    ecs.singleton<EventQueue<SpawnRequest>>().clear();
    ecs.singleton<EventQueue<DespawnRequest>>().clear();
    ecs.singleton<EventQueue<TransformRequest>>().clear();
    ecs.singleton<EventQueue<DeathRequest>>().clear();
    ecs.singleton<EventQueue<RoomRequest>>().clear();
    ecs.singleton<EventQueue<SpawnEvent>>().clear();
    ecs.singleton<EventQueue<ContactEvent>>().clear();
    ecs.singleton<EventQueue<HitEvent>>().clear();
    ecs.singleton<EventQueue<HpEvent>>().clear();
    ecs.singleton<EventQueue<DeathEvent>>().clear();
    ecs.singleton<EventQueue<CastEvent>>().clear();
    ecs.singleton<EventQueue<ActionEvent>>().clear();
    ecs.singleton<EventQueue<StateEvent>>().clear();
    ecs.singleton<EventQueue<BuffEvent>>().clear();
    ecs.singleton<EventQueue<CueEvent>>().clear();
    ecs.singleton<EventQueue<RoomEvent>>().clear();
    ecs.singleton<EventQueue<DespawnEvent>>().clear();
    ecs.singleton<EventQueue<DispEvent>>().clear();
    ecs.singleton<EventQueue<StopEvent>>().clear();
}
}  // namespace

// 系统 FlushSystem：12.2 行，见 DESIGN.md §3.2。
void runFlush(World& world)
{
    // 请求队列中只有 SpawnRequest 在 S0 已有消费者，必须已被同步点排空。
    MG_ASSERT(world.ecs().singleton<EventQueue<SpawnRequest>>().empty() && "SpawnRequest queue must be drained");
    world.ecs().flushDestroyed();
    clearEventQueues(world);
}
}  // namespace mugen::sim
