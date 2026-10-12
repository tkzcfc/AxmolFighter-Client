#pragma once

#include "FixedMath.h"

NS_MG_BEGIN

// 由 Q44.20 坐标组成的三维逻辑向量。
struct FixedVec3
{
    // 水平坐标分量。
    Fixed x;
    // 高度坐标分量。
    Fixed y;
    // 深度坐标分量。
    Fixed z;

    friend constexpr FixedVec3 operator+(FixedVec3 lhs, FixedVec3 rhs)
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }
    friend constexpr FixedVec3 operator-(FixedVec3 lhs, FixedVec3 rhs)
    {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }
    friend FixedVec3 operator*(FixedVec3 value, Fixed scale)
    {
        return {value.x * scale, value.y * scale, value.z * scale};
    }
    friend FixedVec3 operator/(FixedVec3 value, Fixed scale)
    {
        return {value.x / scale, value.y / scale, value.z / scale};
    }
    // 返回避免开方的长度平方。
    Fixed lengthSq() const { return x * x + y * y + z * z; }
    // 返回向下取整的欧氏长度。
    Fixed length() const { return FixedMath::sqrt(lengthSq()); }
};

NS_MG_END
