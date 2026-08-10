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
    Vector2i size;
    std::vector<LayerMoveRange> moveRanges;
};

class LayerLoader
{
public:
    static LayerLoadResult load(const std::string& layerFile);
};

NS_MG_END
