#pragma once
#include "BuffRules.h"

namespace mugen::sim::buff
{
// 调用实例对应的规则钩子；进入条件由生命周期调用者先检查。
void invokeRule(World& world, Entity entity, uint64_t serial, RuleHook hook, Fixed delta = {});
// 重复添加时更新层数和配置，保留旧配置执行叠层回调的顺序。
void resetInstance(World& world, Entity entity, uint64_t serial, const conf::BuffConfig& config);
// 移除指定层数，退出钩子的次数遵循全部移除和强制移层分支。
void removeRepeat(World& world, Entity entity, uint64_t serial, bool all = false, int32_t count = 1, bool force = false);
}
