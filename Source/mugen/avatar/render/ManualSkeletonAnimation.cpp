#include "ManualSkeletonAnimation.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/SpineSkeletonLoader.h"

NS_MG_BEGIN

// 从骨骼/图集文件创建手动驱动节点（缓存共享 SkeletonData，不接管所有权）
ManualSkeletonAnimation* ManualSkeletonAnimation::createWithFile(const std::string& skeletonFile,
                                                                 const std::string& atlasFile,
                                                                 float scale)
{
    SpineSkeletonAssets assets = SpineSkeletonLoader::loadFromFile(skeletonFile, atlasFile, scale);
    if (!assets.valid())
        return nullptr;

    ManualSkeletonAnimation* node = new (std::nothrow) ManualSkeletonAnimation();
    if (!node)
        return nullptr;

    // Shared cache owns atlas + attachment loader + skeleton data
    node->_atlas            = assets.atlas;
    node->_attachmentLoader = assets.attachmentLoader;
    node->_ownsAtlas        = false;
    node->initWithData(assets.skeletonData, false);
    node->autorelease();
    return node;
}

ManualSkeletonAnimation::~ManualSkeletonAnimation()
{
    // SpineSkeletonCache owns these; clear before SkeletonRenderer dtor deletes them.
    _attachmentLoader = nullptr;
    _atlas            = nullptr;
}

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
