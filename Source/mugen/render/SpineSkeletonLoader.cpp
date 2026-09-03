#include "SpineSkeletonLoader.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/Config.h"

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

spine::SkeletonAnimation* SpineSkeletonLoader::createSkeletonAnimation(int32_t skeletonId)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonId);
    if (!data)
        return nullptr;
    return spine::SkeletonAnimation::createWithData(data, false);
}

NS_MG_END

#endif
