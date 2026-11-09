// [10.1] RoomDirectorSystem
// 读：`DeathEvent`、`RoomRequest`、`RoomState`、`Identity`、`Health`
// 写：`RoomState`、`SlowMotion`(KO 慢放)、复活
// 消费：`RoomRequest`
// 产生：`RoomEvent`、`SpawnRequest`(波次)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 RoomDirectorSystem：10.1 行，见 DESIGN.md §3.2。
void runRoomDirector(World&)
{
}
}  // namespace mugen::sim
