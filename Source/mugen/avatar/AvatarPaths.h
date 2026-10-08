#pragma once

#include "mugen/core/StdC.h"

#include <cstdint>
#include <string>

NS_MG_BEGIN

// 角色身体 Spine：只按 role.resSpineId 查 res_spine，不做城镇覆盖或默认缩放猜测。
struct RoleSpineInfo
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
    static RoleSpineInfo resolveRoleSpine(int32_t roleId);
};

NS_MG_END
