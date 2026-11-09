// [4.3s] SpawnFlush(RoleTimeline)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 RoleTimeline：本帧参与 4.3b、P5、P6）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [4.3t] SpawnFlush(EffectTimeline)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 EffectTimeline：下一帧起行动）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [8.s] SpawnFlush(Combat)
// 读：—
// 写：新实体（出生阶段 Combat）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [9.s] SpawnFlush(Lifecycle)
// 读：—
// 写：新实体（出生阶段 Lifecycle）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [10.s] SpawnFlush(Room)
// 读：—
// 写：新实体（出生阶段 Room）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
namespace
{
// 同步点实现：按入队顺序排空 SpawnRequest；S0 尚无实体原型，只丢弃请求。
void spawnFlush(World& world, SpawnStage stage)
{
    static_cast<void>(stage);
    world.ecs().singleton<EventQueue<SpawnRequest>>().clear();
}
}  // namespace

// 系统 SpawnFlush(RoleTimeline)：4.3s 行，见 DESIGN.md §3.2。
void runSpawnFlushRoleTimeline(World& world)
{
    spawnFlush(world, SpawnStage::RoleTimeline);
}
// 系统 SpawnFlush(EffectTimeline)：4.3t 行，见 DESIGN.md §3.2。
void runSpawnFlushEffectTimeline(World& world)
{
    spawnFlush(world, SpawnStage::EffectTimeline);
}
// 系统 SpawnFlush(Combat)：8.s 行，见 DESIGN.md §3.2。
void runSpawnFlushCombat(World& world)
{
    spawnFlush(world, SpawnStage::Combat);
}
// 系统 SpawnFlush(Lifecycle)：9.s 行，见 DESIGN.md §3.2。
void runSpawnFlushLifecycle(World& world)
{
    spawnFlush(world, SpawnStage::Lifecycle);
}
// 系统 SpawnFlush(Room)：10.s 行，见 DESIGN.md §3.2。
void runSpawnFlushRoom(World& world)
{
    spawnFlush(world, SpawnStage::Room);
}
}  // namespace mugen::sim
