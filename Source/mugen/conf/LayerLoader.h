#pragma once

#include "mugen/core/Object.h"
#include "mugen/core/math/Vec2.h"


NS_MG_BEGIN

class MapDataConfig;

struct LayerMoveRange
{
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;
};

struct LayerLoadResult
{
    Vector2i size;
    std::vector<LayerMoveRange> moveRanges;
};

class LayerLoader
{
public:

    // 加载元数据
    static LayerLoadResult load(const std::string& layerFile);

};


NS_MG_END

