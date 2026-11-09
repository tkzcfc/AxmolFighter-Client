// [9.1] DeathSystem
// 读：`DeathRequest`、`Health`、`Identity`、`TransformLink`、`Summon`
// 写：`RoleState`(死亡请求位)、`Combo`(击杀)
// 消费：`DeathRequest`
// 产生：`DeathEvent`、`SpawnRequest`(ko/死亡特效、掉落)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 DeathSystem：9.1 行，见 DESIGN.md §3.2。
void runDeath(World&)
{
}
}  // namespace mugen::sim
