#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec2.h"
#include <string>
#include <vector>

namespace mugen::sim
{
// 地图上一块矩形可移动区域。
struct MoveRange
{
    // 矩形左下角和尺寸。
    Fixed m_x, m_y, m_width, m_height;
    MG_REFLECT(MoveRange, m_x, m_y, m_width, m_height)
};
// 每场世界独有的地图可移动区域集合（移植自旧实现 MapRegion）。
struct MapRegion
{
    // 已加载的地图配置键。
    std::string m_mapKey;
    // 角色可移动的矩形区域。
    std::vector<MoveRange> m_moveRanges;
    // 从地图层文件加载可移动区域。
    void load(const std::string& layerFile);
    // 查询点左侧可通行边界。
    Fixed getLeftRegion(FixedVec2 point) const;
    // 查询点右侧可通行边界。
    Fixed getRightRegion(FixedVec2 point) const;
    // 查询点上侧可通行边界。
    Fixed getTopRegion(FixedVec2 point) const;
    // 查询点下侧可通行边界。
    Fixed getBottomRegion(FixedVec2 point) const;
    MG_REFLECT(MapRegion, m_mapKey, m_moveRanges)
};
}  // namespace mugen::sim
