#pragma once

#include "Fixed.h"

NS_MG_BEGIN

// 提供基于查表和整数算法的确定性定点数学函数。
namespace FixedMath
{

// 返回非负定点数的向下取整平方根。
Fixed sqrt(Fixed value);
// 返回角度制正弦值，输入按 360 度周期归一化。
Fixed sinDeg(Fixed degrees);
// 返回角度制余弦值。
Fixed cosDeg(Fixed degrees);
// 返回 [-180, 180] 范围内的角度制 atan2 结果。
Fixed atan2Deg(Fixed y, Fixed x);

}  // namespace FixedMath

NS_MG_END
