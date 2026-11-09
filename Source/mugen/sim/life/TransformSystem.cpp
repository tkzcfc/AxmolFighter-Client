// [9.2] TransformSystem
// 读：`TransformRequest`
// 写：新形态实体、旧形态加 `Inactive`、`TransformLink`
// 消费：`TransformRequest`
// 产生：`SpawnEvent`、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 TransformSystem：9.2 行，见 DESIGN.md §3.2。
void runTransform(World&)
{
}
}  // namespace mugen::sim
