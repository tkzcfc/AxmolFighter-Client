#pragma once

#include "mugen/core/StdC.h"

#include <cstdint>
#include <string>

NS_MG_BEGIN

// 实体 Spine 外观：由 res_spine 解析出的骨架、图集与缩放，不做城镇覆盖或默认缩放猜测。
struct SpineInfo
{
    bool valid = false;
    std::string skeleton;
    std::string atlas;
    float scale = 1.0f;
};

class AvatarPaths
{
public:
    // spine 骨架路径 → .motion 路径（mugen/spine/x.skel → mugen/motion/x.motion）
    static std::string motionFileFromSpine(const std::string& spineSkeleton);

    // roleId → res_spine 的骨架/atlas/缩放。角色不存在或未配置 Spine 时 valid=false。
    static SpineInfo resolveRoleSpine(int32_t roleId);

    // resSpineId → res_spine 的骨架/atlas/缩放。资源 id 未配置或缺行时 valid=false。
    static SpineInfo resolveSpine(int32_t resSpineId);
};

NS_MG_END
