#pragma once
#include "mugen/core/math/Fixed.h"

namespace mugen::sim_legacy
{
// 可选的抽样策略：为效果摘要导出固定暴击、浮动、闪避与概率抽样（0-100 范围）。
// 由驱动拥有，世界仅持有非拥有指针；不参与快照/哈希；未设置时世界按原有方式消耗随机数。
struct DrawPolicy
{
    // 暴击抽样：101 永不暴击，0 必暴击。
    Fixed crit = Fixed::fromInt(101);
    // 伤害浮动抽样：50 对应浮动系数 1.00。
    Fixed floatDraw = Fixed::fromInt(50);
    // 属性闪避抽样：100 永不属性闪避。
    Fixed dodge = Fixed::fromInt(100);
    // Buff、技能 AI、能力等概率类抽样：0 总是通过。
    Fixed probability = Fixed::fromInt(0);
};
}  // namespace mugen::sim_legacy
