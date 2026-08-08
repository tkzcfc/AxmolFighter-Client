#pragma once

#include "mugen/avatar/AvatarLayerDef.h"

#include <string>
#include <vector>

NS_MG_BEGIN

class AvatarComponent;

class AvatarLayerUtils
{
public:
    static std::string resolveAssetPath(const std::string& baseDir, const std::string& fileName);
    static std::string spinePathToBoxDir(const std::string& spineSkeleton);
    static std::vector<AvatarLayerDef> resolveLayers(const AvatarComponent* avatar);
    static std::vector<AvatarLayerDef> resolveLayersFromSpine(const std::string& spineSkeleton,
                                                              const std::string& motionFile,
                                                              const std::string& defaultAnimationPath);
};

NS_MG_END
