#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/SpineSkeletonCache.h"

#    include "spine/SkeletonAnimation.h"

NS_MG_BEGIN

// 薄封装：经缓存取共享 SkeletonData，再创建节点。
class SpineSkeletonLoader
{
public:
    // 经缓存加载（按内容探测 JSON / Binary）；返回非拥有指针
    static SpineSkeletonAssets loadFromFile(const std::string& skeletonFile,
                                            const std::string& atlasFile,
                                            float scale = 1.0f);

    // 创建会自动 scheduleUpdate 的 SkeletonAnimation（地图装饰用）
    static spine::SkeletonAnimation* createSkeletonAnimation(const std::string& skeletonFile,
                                                             const std::string& atlasFile,
                                                             float scale = 1.0f);
};

NS_MG_END

#endif
