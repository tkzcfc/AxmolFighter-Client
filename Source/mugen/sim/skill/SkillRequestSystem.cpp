// [4.1] SkillRequestSystem
// 读：`Intent`、`SkillBook`、`SkillCast`、`Stats`、`Health`、`StatusFlags`、`RoleState`
// 写：`SkillCast`（当前施法/缓存下一招/nextRun）、`SkillBook`
// 消费：—
// 产生：`CastEvent`(Prepared)；同步触发 BeforePrepareSkill
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 SkillRequestSystem：4.1 行，见 DESIGN.md §3.2。
void runSkillRequest(World&)
{
}
}  // namespace mugen::sim
