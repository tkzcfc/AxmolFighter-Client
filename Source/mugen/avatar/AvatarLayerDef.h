#pragma once

#include "mugen/core/Object.h"

#include <cstdint>
#include <string>

NS_MG_BEGIN

// 非装备层标记（角色本体）
inline constexpr int32_t kAvatarLayerTagCharacter = -1;

// Avatar 单层静态描述（逻辑与渲染共用，可序列化）
class AvatarLayerDef : public Object
{
public:
    typedef Object Super;

    // .motion 文件路径（Content 相对）
    std::string motionMapPath;
    // 资源基目录（解析 .box）
    std::string baseDir;
    // 渲染 LocalZOrder
    int32_t order = 0;
    // 来源：非角色本体层的来源标记，默认 kAvatarLayerTagCharacter
    int32_t tag = kAvatarLayerTagCharacter;

    MG_DEFINE_SERIALIZABLE(motionMapPath, baseDir, order, tag);
};

NS_MG_END
