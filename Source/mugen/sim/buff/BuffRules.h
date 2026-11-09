#pragma once
#include "mugen/sim/buff/BuffComponents.h"
#include "mugen/core/ecs/Entity.h"
#include <vector>

namespace mugen::sim
{
class World;
namespace buff::rules
{
// 从 buff 配置降级常驻 op（S1-B3 只识别霸体 ruleId 1，其余规则留到 S2）。
std::vector<BuffOp> lowerOps(int32_t ruleId);
// 准入一个 Buff 实例：执行 Enter op，发出 BuffEvent Added。
void admit(World& world, Entity entity, int32_t buffId);
// 移除指定 id 的全部实例：撤销常驻 op，发出 BuffEvent Removed。
void remove(World& world, Entity entity, int32_t buffId);
}  // namespace buff::rules
}  // namespace mugen::sim
