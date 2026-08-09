#pragma once

#include "mugen/core/Object.h"

#ifdef RUNTIME_IN_AXMOL

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
    ax::ParallaxNode* root = nullptr;
    ax::Size rootSize;
    std::vector<LayerMoveRange> moveRanges;
};

class LayerRuntimeLoader
{
public:
    static LayerLoadResult load(const std::string& layerFile, const MapDataConfig* mapData = nullptr);
};


NS_MG_END

#endif
