#pragma once

#include "mugen/core/Object.h"

#include <string>
#include <vector>

namespace ax
{
class Node;
class ParallaxNode;
}  // namespace ax

NS_MG_BEGIN

class MapDataConfig;

#ifdef RUNTIME_IN_AXMOL

struct LayerMoveRange
{
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;
};

struct LayerLoadResult
{
    // 实际为 ParallaxNode；九层已按 MapDataConfig offset 挂好
    ax::ParallaxNode* root = nullptr;
    ax::Size rootSize;
    std::vector<LayerMoveRange> moveRanges;
};

class LayerRuntimeLoader
{
public:
    // 加载九层场景为 ParallaxNode 根；跳过编辑器 Parallax Object；视差 ratio 来自 mapData
    static LayerLoadResult load(const std::string& layerFile, const MapDataConfig* mapData = nullptr);

    // 兼容旧调用：仅返回节点树
    static ax::Node* createNodeTree(const std::string& layerFile);
};

#endif

NS_MG_END
