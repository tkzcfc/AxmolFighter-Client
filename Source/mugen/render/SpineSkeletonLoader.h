#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/SpineSkeletonCache.h"

#    include "spine/SkeletonAnimation.h"

NS_MG_BEGIN

class SpineSkeletonLoader
{
public:
    static spine::SkeletonAnimation* createSkeletonAnimation(const std::string& skeletonFile,
                                                             const std::string& atlasFile,
                                                             float scale = 1.0f);
};

NS_MG_END

#endif
