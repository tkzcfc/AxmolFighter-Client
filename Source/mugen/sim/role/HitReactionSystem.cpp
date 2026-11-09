// [4.4] HitReactionSystem
// 读：`RoleState`（受击族、死亡）、`HitState`、`Displacement` 事件位
// 写：`HitState`（硬直/倒地计时）、`Displacement`（落地反弹策略）、挣脱请求
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 HitReactionSystem：4.4 行，见 DESIGN.md §3.2。
void runHitReaction(World&)
{
}
}  // namespace mugen::sim
