#pragma once
#include "mugen/sim/anim/AnimComponents.h"
#include "mugen/sim/motion/MotionComponents.h"
#include "mugen/core/math/DamageBox.h"

namespace mugen::sim
{
// 把局部框放到世界坐标。X 为横向，Y 为纵深，Z 为高度。
// depthFull > 0 时盖过文件深度：全长、不乘缩放、绕 position.z 居中。
inline WorldBox placeCombatBox(const DamageBox& box, const TransformComponent& transform, Fixed depthFull)
{
    WorldBox result;
    const Fixed sizeX  = Fixed::fromInt(box.size.x) * transform.scale;
    const Fixed localX = Fixed::fromInt(box.pos.x) * transform.scale;
    result.size.x    = sizeX;
    result.pos.x     = transform.position.x + (transform.towardX == -1 ? -localX - sizeX : localX);
    result.size.z    = Fixed::fromInt(box.size.z) * transform.scale;
    result.pos.z     = transform.position.y + Fixed::fromInt(box.pos.z) * transform.scale;
    if (depthFull > Fixed{})
    {
        result.size.y = depthFull;
        result.pos.y  = transform.position.z - depthFull * Fixed::ratio(1, 2);
    }
    else
    {
        result.size.y = Fixed::fromInt(box.size.y) * transform.scale;
        result.pos.y  = transform.position.z + Fixed::fromInt(box.pos.y) * transform.scale;
    }
    return result;
}

// 三轴半开区间的重叠起点。无重叠时返回 false。
inline bool overlapStart(Fixed pos, Fixed size, Fixed otherPos, Fixed otherSize, Fixed& start, Fixed& end)
{
    start = pos > otherPos ? pos : otherPos;
    const Fixed endA = pos + size;
    const Fixed endB = otherPos + otherSize;
    end              = endA < endB ? endA : endB;
    return start < end;
}

// 世界盒在横向、纵深、高度上是否都有正长度交集。point 写出重叠中心，坐标与 Transform 一致（x 横向、y 高度、z 纵深）。
inline bool worldBoxesOverlap(const WorldBox& first, const WorldBox& second, FixedVec3* point)
{
    Fixed x0, x1, y0, y1, z0, z1;
    if (!overlapStart(first.pos.x, first.size.x, second.pos.x, second.size.x, x0, x1) ||
        !overlapStart(first.pos.y, first.size.y, second.pos.y, second.size.y, y0, y1) ||
        !overlapStart(first.pos.z, first.size.z, second.pos.z, second.size.z, z0, z1))
        return false;
    if (point)
        *point = {(x0 + x1) * Fixed::ratio(1, 2), (z0 + z1) * Fixed::ratio(1, 2), (y0 + y1) * Fixed::ratio(1, 2)};
    return true;
}
}  // namespace mugen::sim
