#include "ManualSkeletonAnimation.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/SpineSkeletonCache.h"

NS_MG_BEGIN

// 从骨骼/图集文件创建手动驱动节点（缓存共享 SkeletonData，不接管所有权）
ManualSkeletonAnimation* ManualSkeletonAnimation::createWithFile(const std::string& skeletonFile,
                                                                 const std::string& atlasFile,
                                                                 float scale)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, atlasFile, scale);
    if (!data)
        return nullptr;

    ManualSkeletonAnimation* node = new (std::nothrow) ManualSkeletonAnimation();
    if (!node)
        return nullptr;

    node->initWithData(data, false);
    node->autorelease();
    return node;
}

ManualSkeletonAnimation::~ManualSkeletonAnimation() = default;

void ManualSkeletonAnimation::onEnter()
{
    ax::Node::onEnter();
}

void ManualSkeletonAnimation::onExit()
{
    ax::Node::onExit();
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
