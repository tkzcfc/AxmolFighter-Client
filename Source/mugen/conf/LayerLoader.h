#pragma once

#include "mugen/core/math/IntVec2.h"
#include "mugen/core/math/Fixed.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 地图中允许移动的矩形范围。
struct LayerMoveRange
{
    // 矩形左下角横坐标。
    Fixed x;
    // 矩形左下角纵坐标。
    Fixed y;
    // 矩形宽度。
    Fixed width;
    // 矩形高度。
    Fixed height;
};

// 从地图层文件提取的逻辑元数据。
struct LayerLoadResult
{
    // 地图的大小
    IntVec2 size;
    // 移动范围
    std::vector<LayerMoveRange> moveRanges;
    // 音效id
    int32_t soundId = 0;
};

// 解析地图层 JSON 的无状态加载器。
class LayerLoader
{
public:
    // 读取层文件并提取尺寸、移动范围和音效。
    static LayerLoadResult load(const std::string& layerFile);
};

NS_MG_END
