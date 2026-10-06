#pragma once

#include "mugen/core/Object.h"

#ifdef RUNTIME_IN_AXMOL

#    include <cstdint>
#    include <string>
#    include <utility>
#    include <vector>

NS_MG_BEGIN

// 一个 .layer 文件引用到的全部资源，只做纯数据解析，不创建任何节点。
// 用于预加载：和 LayerRuntimeLoader::loadNode 共用同一套 JSON 结构解析逻辑。
struct LayerResourceList
{
    std::vector<std::pair<std::string, std::string>> spines;  // {jsonPath, atlasPath}；加载时缩放固定为 1.0
    std::vector<std::string> textures;                        // Sprite(sourceType=Texture) 的 imagePath
    std::vector<std::string> spriteFrames;                    // Sprite(sourceType=SpriteFrame) 的 atlasPath（plist）
    int32_t soundId = 0;                                      // meta.soundId，背景音乐
};

class LayerRuntimeLoader
{
public:
    // 构建九层视觉树；同时从 Root/meta Object 读取视差 offset（跳过 type==Object 节点）
    static ax::ParallaxNode* loadNode(const std::string& layerFile);

    // 只解析、不创建节点；供预加载使用
    static LayerResourceList collectResources(const std::string& layerFile);
};

NS_MG_END

#endif
