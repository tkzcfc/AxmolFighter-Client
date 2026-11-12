// [4.3s] SpawnFlushSystem(RoleTimeline)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 RoleTimeline：本帧参与 4.3b、P5、P6）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [4.3t] SpawnFlushSystem(EffectTimeline)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 EffectTimeline：下一帧起行动）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [8.s] SpawnFlushSystem(Combat)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 Combat）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [9.s] SpawnFlushSystem(Lifecycle)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 Lifecycle）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
// [10.s] SpawnFlushSystem(Room)
// 读：`SpawnRequest`
// 写：新实体（出生阶段 Room）
// 消费：`SpawnRequest`
// 产生：`SpawnEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/life/EntityFactory.h"

namespace mugen::sim
{
// 系统 SpawnFlushSystem(RoleTimeline)：4.3s 行，见 DESIGN.md §3.2。
void runSpawnFlushRoleTimeline(World& world)
{
    life::rules::spawnFlush(world, SpawnStage::RoleTimeline);
}
// 系统 SpawnFlushSystem(EffectTimeline)：4.3t 行，见 DESIGN.md §3.2。
void runSpawnFlushEffectTimeline(World& world)
{
    life::rules::spawnFlush(world, SpawnStage::EffectTimeline);
}
// 系统 SpawnFlushSystem(Combat)：8.s 行，见 DESIGN.md §3.2。
void runSpawnFlushCombat(World& world)
{
    life::rules::spawnFlush(world, SpawnStage::Combat);
}
// 系统 SpawnFlushSystem(Lifecycle)：9.s 行，见 DESIGN.md §3.2。
void runSpawnFlushLifecycle(World& world)
{
    life::rules::spawnFlush(world, SpawnStage::Lifecycle);
}
// 系统 SpawnFlushSystem(Room)：10.s 行，见 DESIGN.md §3.2。
void runSpawnFlushRoom(World& world)
{
    life::rules::spawnFlush(world, SpawnStage::Room);
}
}  // namespace mugen::sim
