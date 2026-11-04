#pragma once

#include "mugen/core/math/Fixed.h"

#include <cmath>
#include <cstdint>

namespace gameui
{

// 城镇同步的定点坐标约定：线路上用 int32 定点（px = 值 / 1000），
// 这里提供它与 mugen::Fixed / float 之间的确定性转换。

// 渲染/逻辑像素 → 线路定点（四舍五入）。
inline int32_t townToFixedPoint(float px)
{
    return static_cast<int32_t>(std::llround(px * 1000.0f));
}

// 线路定点 → mugen::Fixed（px = 值 / 1000）。
inline mugen::Fixed townFromFixedPoint(int32_t v)
{
    return mugen::Fixed::fromRaw(static_cast<int64_t>(std::llround(v / 1000.0 * mugen::Fixed::kOne)));
}

// 像素浮点 → mugen::Fixed（插值输出用）。
inline mugen::Fixed townPxToFixed(float px)
{
    return mugen::Fixed::fromRaw(static_cast<int64_t>(std::llround(px * static_cast<double>(mugen::Fixed::kOne))));
}

}  // namespace gameui
