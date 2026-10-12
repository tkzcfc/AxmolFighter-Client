#pragma once

#include "FixedMath.h"

NS_MG_BEGIN

// 由 Q44.20 坐标组成的二维逻辑向量。
struct FixedVec2
{
    // 水平坐标分量。
    Fixed x;
    // 垂直坐标分量。
    Fixed y;

    friend constexpr FixedVec2 operator+(FixedVec2 lhs, FixedVec2 rhs) { return {lhs.x + rhs.x, lhs.y + rhs.y}; }
    friend constexpr FixedVec2 operator-(FixedVec2 lhs, FixedVec2 rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y}; }
    friend FixedVec2 operator*(FixedVec2 value, Fixed scale) { return {value.x * scale, value.y * scale}; }
    friend FixedVec2 operator/(FixedVec2 value, Fixed scale) { return {value.x / scale, value.y / scale}; }
    // 返回避免开方的长度平方。
    Fixed lengthSq() const { return x * x + y * y; }
    // 返回向下取整的欧氏长度。
    Fixed length() const { return FixedMath::sqrt(lengthSq()); }
};

NS_MG_END
