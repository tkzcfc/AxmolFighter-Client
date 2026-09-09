#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/SpineRuntime.h"
#    include "mugen/render/SpineSkeletonCache.h"

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
