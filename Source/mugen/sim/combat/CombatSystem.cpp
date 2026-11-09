// [8.1] CombatSystem
// 读：`ContactEvent`、`EffectInfo`、`HitTracker`、`HitDef`、`Stats`、`BuffList`、`StatusFlags`、`RoleState`、`SkillCast`
// 写：`HitTracker`、`Health`、`BuffList`(同步准入)、`Pause`、`HitState`(受击请求与记录)、`SkillCast`(强制打断)、`Combo`、`Rage`
// 消费：`ContactEvent`(特效→角色/水晶)
// 产生：`HitEvent`、`HpEvent`、`DeathRequest`、`SpawnRequest`(受击特效)、`DespawnRequest`、`CueEvent`；RNG
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 CombatSystem：8.1 行，见 DESIGN.md §3.2。
void runCombat(World&)
{
}
}  // namespace mugen::sim
