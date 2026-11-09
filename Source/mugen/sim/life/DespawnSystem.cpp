// [9.3] DespawnSystem
// 读：`Despawning`、`Lifetime`、`EffectInfo`、`DespawnRequest`
// 写：主人消失时的随主特效、Buff 清理、`ScheduledActions` 取消
// 消费：`DespawnRequest`
// 产生：`SpawnRequest`(后继特效)、`DespawnEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 DespawnSystem：9.3 行，见 DESIGN.md §3.2。
void runDespawn(World&)
{
}
}  // namespace mugen::sim
