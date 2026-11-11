// [2.2] BuffTickSystem
// 读：`BuffList`、`AbilityList`、`BuffDef`
// 写：两个列表的计时；经 op 写 `Stats`/`Health`/`StatusFlags`/`SkillBook`
// 消费：—
// 产生：`BuffEvent`、`HpEvent`、`DeathRequest`、`SpawnRequest`、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"

namespace mugen::sim
{
// 系统 BuffTickSystem：2.2 行。周期、冷却、监视与特殊能力都在顿帧期间继续。
void runBuffTick(World& world)
{
    buff::rules::tick(world);
}
}  // namespace mugen::sim
