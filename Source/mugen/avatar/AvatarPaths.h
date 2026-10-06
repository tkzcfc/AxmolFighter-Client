#pragma once

#include "mugen/core/StdC.h"

#include <string>

NS_MG_BEGIN

class AvatarPaths
{
public:
    // spine 骨架路径 → .motion 路径（mugen/spine/x.skel → mugen/motion/x.motion）
    static std::string motionFileFromSpine(const std::string& spineSkeleton);
};

NS_MG_END
