#pragma once

#include "mugen/avatar/AvatarLayerDef.h"

#include <string>
#include <vector>

NS_MG_BEGIN

class AvatarComponent;

class AvatarLayerUtils
{
public:
    static std::string spinePathToMotionFile(const std::string& spineSkeleton);
    static std::vector<AvatarLayerDef> resolveLayers(const AvatarComponent* avatar);
    static std::vector<AvatarLayerDef> resolveLayersFromSpine(const std::string& motionFile);
};

NS_MG_END
