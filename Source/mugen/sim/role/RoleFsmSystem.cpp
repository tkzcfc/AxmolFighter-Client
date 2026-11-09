// [4.2] RoleFsmSystem
// 读：`RoleState`、`SkillCast`、`Intent.m_move`、`HitState`、`Health`、`Displacement` 事件位、`MotionState` 完成位
// 写：`RoleState`；进入/退出动作写 `MotionState`、`Displacement`、`SkillCast`、`ActionTimeline`（经 `skill::rules::beginCast/endCast`）
// 消费：—
// 产生：`StateEvent`、`CastEvent`(Began/Ended)；同步触发施法、连招、行为状态、ColdStart、UseTp/UseEp
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 RoleFsmSystem：4.2 行，见 DESIGN.md §3.2。
void runRoleFsm(World&)
{
}
}  // namespace mugen::sim
