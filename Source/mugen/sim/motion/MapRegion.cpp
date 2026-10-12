#include "MapRegion.h"
#include "mugen/conf/LayerLoader.h"

namespace mugen::sim
{
// 清空旧区域并从层文件复制移动范围。
void MapRegion::load(const std::string& layerFile)
{
    moveRanges.clear();
    for (const auto& range : LayerLoader::load(layerFile).moveRanges)
        moveRanges.push_back({range.x, range.y, range.width, range.height});
}
namespace
{
// 在指定轴向查找点所在或最近区域的边界。
Fixed boundary(const MapRegion& region, FixedVec2 point, bool horizontal, bool lower)
{
    const Fixed value = horizontal ? point.x : point.y;
    const Fixed cross = horizontal ? point.y : point.x;
    Fixed fallback    = value;
    Fixed result      = value;
    bool matched      = false;
    for (const auto& range : region.moveRanges)
    {
        const Fixed start  = horizontal ? range.y : range.x;
        const Fixed length = horizontal ? range.height : range.width;
        const Fixed edge   = (horizontal ? range.x : range.y) + (lower        ? Fixed{}
                                                                 : horizontal ? range.width
                                                                              : range.height);
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
