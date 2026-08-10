#include "SpineSkeletonLoader.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

spine::SkeletonAnimation* SpineSkeletonLoader::createSkeletonAnimation(const std::string& skeletonFile,
                                                                       const std::string& atlasFile,
                                                                       float scale)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, atlasFile, scale);
    if (!data)
        return nullptr;
    return spine::SkeletonAnimation::createWithData(data, false);
}

NS_MG_END

#endif
