#pragma once

#include "FixedMath.h"

NS_MG_BEGIN

struct FixedVec3
{
    Fixed x;
    Fixed y;
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
    Fixed lengthSq() const { return x * x + y * y + z * z; }
    Fixed length() const { return FixedMath::sqrt(lengthSq()); }
};

NS_MG_END
