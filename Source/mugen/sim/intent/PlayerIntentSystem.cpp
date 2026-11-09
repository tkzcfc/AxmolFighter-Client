// [3.2] PlayerIntentSystem
// 读：`PlayerControl`、`StatusFlags`(canControl)、`Pause`
// 写：`Intent`；未暂停时排空 `PlayerControl.m_commands`
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 PlayerIntentSystem：3.2 行，见 DESIGN.md §3.2。
void runPlayerIntent(World&)
{
}
}  // namespace mugen::sim
