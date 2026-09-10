#include "SpineSkeletonLoader.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

MgSkeletonAnimation* SpineSkeletonLoader::createSkeletonAnimation(const std::string& skeletonFile,
                                                                  const std::string& atlasFile,
                                                                  float scale)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, atlasFile, scale);
    if (!data)
        return nullptr;
    return MgSkeletonAnimation::createWithData(data);
}

MgSkeletonAnimation* SpineSkeletonLoader::createSkeletonAnimation(int32_t skeletonId)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonId);
    if (!data)
        return nullptr;
    return MgSkeletonAnimation::createWithData(data);
}

NS_MG_END

#endif
