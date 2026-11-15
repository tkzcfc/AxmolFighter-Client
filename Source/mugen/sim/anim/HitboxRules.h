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
    const Fixed sizeX  = Fixed::fromInt(box.size.x) * transform.m_scale;
    const Fixed localX = Fixed::fromInt(box.pos.x) * transform.m_scale;
    result.m_size.x    = sizeX;
    result.m_pos.x     = transform.m_position.x + (transform.m_towardX == -1 ? -localX - sizeX : localX);
    result.m_size.z    = Fixed::fromInt(box.size.z) * transform.m_scale;
    result.m_pos.z     = transform.m_position.y + Fixed::fromInt(box.pos.z) * transform.m_scale;
    if (depthFull > Fixed{})
    {
        result.m_size.y = depthFull;
        result.m_pos.y  = transform.m_position.z - depthFull * Fixed::ratio(1, 2);
    }
    else
    {
        result.m_size.y = Fixed::fromInt(box.size.y) * transform.m_scale;
        result.m_pos.y  = transform.m_position.z + Fixed::fromInt(box.pos.y) * transform.m_scale;
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
    if (!overlapStart(first.m_pos.x, first.m_size.x, second.m_pos.x, second.m_size.x, x0, x1) ||
        !overlapStart(first.m_pos.y, first.m_size.y, second.m_pos.y, second.m_size.y, y0, y1) ||
        !overlapStart(first.m_pos.z, first.m_size.z, second.m_pos.z, second.m_size.z, z0, z1))
        return false;
    if (point)
        *point = {(x0 + x1) * Fixed::ratio(1, 2), (z0 + z1) * Fixed::ratio(1, 2), (y0 + y1) * Fixed::ratio(1, 2)};
    return true;
}
}  // namespace mugen::sim
