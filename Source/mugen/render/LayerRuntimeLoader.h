#pragma once

#include "mugen/core/Object.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class MapDataConfig;

class LayerRuntimeLoader
{
public:
    static ax::ParallaxNode* loadNode(const std::string& layerFile, const MapDataConfig* mapData = nullptr);
};

NS_MG_END

#endif
