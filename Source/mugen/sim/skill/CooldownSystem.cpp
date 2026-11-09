// [2.1] CooldownSystem
// 读：`SkillBook`、`Stats`(CD 缩放)
// 写：`SkillBook`（CD、次数、SkillAi 计时、技能组/变化计时）
// 消费：—
// 产生：`CastEvent`(ColdEnd)；同步触发 SkillColdEnd
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 CooldownSystem：2.1 行，见 DESIGN.md §3.2。
void runCooldown(World&)
{
}
}  // namespace mugen::sim
