// [2.3] VitalsTickSystem
// 读：`Health`、`Stats`、`SkillBook`(曝气)、`Rage`、`Summon`、`HitState`、`Combo`
// 写：MP 回复、曝气 EP 消耗、怒气冻结、狂暴计时、受击保护计时、连击超时与衰减、召唤寿命、`MoveClock`、生命/能量采样
// 消费：—
// 产生：`DeathRequest`(召唤到期)、`CastEvent`(EpZero/AfterCrazy)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 VitalsTickSystem：2.3 行，见 DESIGN.md §3.2。
void runVitalsTick(World&)
{
}
}  // namespace mugen::sim
