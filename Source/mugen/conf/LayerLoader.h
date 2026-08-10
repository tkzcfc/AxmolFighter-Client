#pragma once

#include "mugen/core/Object.h"
#include "mugen/core/math/Vec2.h"

NS_MG_BEGIN

struct LayerMoveRange
{
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;
};

struct LayerLoadResult
{
    // 地图的大小
    Vector2i size;
    // 移动范围
    std::vector<LayerMoveRange> moveRanges;
    // 音效id
    int32_t soundId = 0;
};

class LayerLoader
{
public:
    static LayerLoadResult load(const std::string& layerFile);
};

NS_MG_END
