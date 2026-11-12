// [8.3] InteractSystem
// 读：`ContactEvent`(物品、传送门)
// 写：`GoodsState`、`PortalState`、`BuffListComponent`(拾取)
// 消费：`ContactEvent`
// 产生：`RoomRequest`(传送)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 InteractSystem：8.3 行，见 DESIGN.md §3.2。
void runInteract(World&)
{
}
}  // namespace mugen::sim
