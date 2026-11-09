// [4.3a] TimelineSystem(roles)
// 读：`ActionTimeline`、`SkillCast`、`MotionState` 完成位、`Stats`(动作速率)
// 写：`ActionTimeline`、`SkillCast`（取消窗口、切招）、`Transform`（瞬移/转向）、`Displacement`（片段位移）
// 消费：—
// 产生：`SpawnRequest`、`TransformRequest`、`CueEvent`、`ActionEvent`；同步：片段 Buff、BeforeRoleSkillAction/After
// [4.3b] TimelineSystem(effects)
// 读：同上（特效实体）
// 写：同上；特效片段放完时打 `Despawning`
// 消费：—
// 产生：`SpawnRequest`（子特效、后继特效、召唤）、`DespawnRequest`、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 TimelineSystem(roles)：4.3a 行，见 DESIGN.md §3.2。
void runTimelineRoles(World&)
{
}
// 系统 TimelineSystem(effects)：4.3b 行，见 DESIGN.md §3.2。
void runTimelineEffects(World&)
{
}
}  // namespace mugen::sim
