#pragma once
#include "mugen/core/math/FixedVec2.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim
{
// 地图上一块矩形可移动区域。
struct MoveRange
{
    // 矩形左下角和尺寸。
    Fixed x, y, width, height;
    // 参与地图状态快照的区域字段。
    MG_REFLECT(MoveRange, x, y, width, height)
};
// 每场世界独有的地图可移动区域集合。
struct MapRegion
{
    // 已加载的地图配置键。
    std::string mapKey;
    // 角色可移动的矩形区域。
    std::vector<MoveRange> moveRanges;
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
    // 参与世界快照的地图字段。
    MG_REFLECT(MapRegion, mapKey, moveRanges)
};
}  // namespace mugen::sim
