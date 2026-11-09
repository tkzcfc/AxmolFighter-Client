#include "mugen/sim/motion/MapRegion.h"
#include "mugen/conf/LayerLoader.h"

namespace mugen::sim
{
// 清空旧区域并从层文件复制移动范围。
void MapRegion::load(const std::string& layerFile)
{
    m_moveRanges.clear();
    for (const auto& range : LayerLoader::load(layerFile).moveRanges)
        m_moveRanges.push_back({range.x, range.y, range.width, range.height});
}
namespace
{
// 在指定轴向查找点所在或最近区域的边界。
Fixed boundary(const MapRegion& region, FixedVec2 point, bool horizontal, bool lower)
{
    for (const auto& range : region.m_moveRanges)
    {
        const bool insideX = point.x >= range.m_x && point.x <= range.m_x + range.m_width;
        const bool insideY = point.y >= range.m_y && point.y <= range.m_y + range.m_height;
        if (insideX && insideY)
            return horizontal ? point.x : point.y;
    }
    const Fixed value = horizontal ? point.x : point.y;
    const Fixed cross = horizontal ? point.y : point.x;
    Fixed fallback    = value;
    Fixed result      = value;
    bool matched      = false;
    for (const auto& range : region.m_moveRanges)
    {
        const Fixed start  = horizontal ? range.m_y : range.m_x;
        const Fixed length = horizontal ? range.m_height : range.m_width;
        const Fixed edge   = (horizontal ? range.m_x : range.m_y) +
                           (lower ? Fixed{} : horizontal ? range.m_width : range.m_height);
        if (cross >= start && cross <= start + length)
        {
            matched = true;
            if ((lower ? value < edge : value > edge) && (result == value || (lower ? edge < result : edge > result)))
                result = edge;
        }
        else if (fallback == value || (lower ? edge < fallback : edge > fallback))
            fallback = edge;
    }
    if (matched)
        return result;
    return (lower ? value < fallback : value > fallback) ? fallback : value;
}
}  // namespace
// 返回点左侧边界。
Fixed MapRegion::getLeftRegion(FixedVec2 point) const
{
    return boundary(*this, point, true, true);
}
// 返回点右侧边界。
Fixed MapRegion::getRightRegion(FixedVec2 point) const
{
    return boundary(*this, point, true, false);
}
// 返回点上侧边界。
Fixed MapRegion::getTopRegion(FixedVec2 point) const
{
    return boundary(*this, point, false, false);
}
// 返回点下侧边界。
Fixed MapRegion::getBottomRegion(FixedVec2 point) const
{
    return boundary(*this, point, false, true);
}
}  // namespace mugen::sim
