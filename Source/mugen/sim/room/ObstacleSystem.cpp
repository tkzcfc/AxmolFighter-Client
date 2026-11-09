// [8.2] ObstacleSystem
// 读：`ContactEvent`(特效→障碍、角色↔障碍)
// 写：`ObstacleState`、`Transform`(推回)
// 消费：`ContactEvent`
// 产生：`DespawnRequest`、`SpawnRequest`(掉落)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 ObstacleSystem：8.2 行，见 DESIGN.md §3.2。
void runObstacle(World&)
{
}
}  // namespace mugen::sim
