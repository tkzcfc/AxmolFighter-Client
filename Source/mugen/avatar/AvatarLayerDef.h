#pragma once

#include "mugen/core/Object.h"

#include <cstdint>
#include <string>

NS_MG_BEGIN

enum class AvatarLayerTag : int32_t
{
    kBody = -1,  // 角色形象合成层
};

// Avatar 单层静态描述（逻辑与渲染共用，可序列化）
class AvatarLayerDef : public Object
{
public:
    typedef Object Super;

    // .motion 文件路径（Content 相对）
    std::string motionMapPath;
    // 渲染 LocalZOrder
    int32_t order = 0;
    // 角色形象合成层标记
    AvatarLayerTag tag = AvatarLayerTag::kBody;

    MG_DEFINE_SERIALIZABLE(motionMapPath, order, tag);
};

NS_MG_END
