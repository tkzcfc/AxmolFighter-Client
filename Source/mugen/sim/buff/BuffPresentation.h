#pragma once
#include "mugen/core/math/Fixed.h"
#include <optional>

namespace mugen::sim::buff
{
// 返回已绑定逻辑动作的毫秒时长；缺失时返回空值而不是猜测。
std::optional<Fixed> disappearDuration(int32_t spineId);
}  // namespace mugen::sim::buff
