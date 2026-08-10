#pragma once

#include "mugen/core/Object.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class LayerRuntimeLoader
{
public:
    // 构建九层视觉树；同时从 Root/meta Object 读取视差 offset（跳过 type==Object 节点）
    static ax::ParallaxNode* loadNode(const std::string& layerFile);
};

NS_MG_END

#endif
