#pragma once

#include "mugen/render/spine/MgSkeletonAnimation.h"
#include "mugen/render/spine/SpineSkeletonCache.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class SpineSkeletonLoader
{
public:
    static MgSkeletonAnimation* createSkeletonAnimation(const std::string& skeletonFile,
                                                        const std::string& atlasFile,
                                                        float scale = 1.0f);

    static MgSkeletonAnimation* createSkeletonAnimation(int32_t skeletonId);
};

NS_MG_END

#endif
