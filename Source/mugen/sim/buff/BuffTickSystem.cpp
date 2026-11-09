// [2.2] BuffTickSystem
// 读：`BuffList`、`AbilityList`、`BuffDef`
// 写：两个列表的计时；经 op 写 `Stats`/`Health`/`StatusFlags`/`SkillBook`
// 消费：—
// 产生：`BuffEvent`、`HpEvent`、`DeathRequest`、`SpawnRequest`、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 BuffTickSystem：2.2 行，见 DESIGN.md §3.2。
void runBuffTick(World&)
{
}
}  // namespace mugen::sim
