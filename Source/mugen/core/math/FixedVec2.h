#pragma once

#include "FixedMath.h"

NS_MG_BEGIN

struct FixedVec2
{
    Fixed x;
    Fixed y;

    friend constexpr FixedVec2 operator+(FixedVec2 lhs, FixedVec2 rhs) { return {lhs.x + rhs.x, lhs.y + rhs.y}; }
    friend constexpr FixedVec2 operator-(FixedVec2 lhs, FixedVec2 rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y}; }
    friend FixedVec2 operator*(FixedVec2 value, Fixed scale) { return {value.x * scale, value.y * scale}; }
    friend FixedVec2 operator/(FixedVec2 value, Fixed scale) { return {value.x / scale, value.y / scale}; }
    Fixed lengthSq() const { return x * x + y * y; }
    Fixed length() const { return FixedMath::sqrt(lengthSq()); }
};

NS_MG_END
