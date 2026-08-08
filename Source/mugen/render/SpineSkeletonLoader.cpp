#include "SpineSkeletonLoader.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

namespace
{

// 使用缓存共享资源创建节点（不接管所有权）
class SharedSkeletonAnimation : public spine::SkeletonAnimation
{
public:
    static SharedSkeletonAnimation* createWithAssets(const SpineSkeletonAssets& assets)
    {
        if (!assets.valid())
            return nullptr;

        auto* node = new (std::nothrow) SharedSkeletonAnimation();
        if (!node)
            return nullptr;

        node->_atlas            = assets.atlas;
        node->_attachmentLoader = assets.attachmentLoader;
        node->_ownsAtlas        = false;
        node->initWithData(assets.skeletonData, false);
        node->autorelease();
        return node;
    }

protected:
    SharedSkeletonAnimation() = default;
    ~SharedSkeletonAnimation() override
    {
        // SpineSkeletonCache owns these; clear before SkeletonRenderer dtor deletes them.
        _attachmentLoader = nullptr;
        _atlas            = nullptr;
    }
};

}  // namespace

SpineSkeletonAssets SpineSkeletonLoader::loadFromFile(const std::string& skeletonFile,
                                                      const std::string& atlasFile,
                                                      float scale)
{
    return SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, atlasFile, scale);
}

spine::SkeletonAnimation* SpineSkeletonLoader::createSkeletonAnimation(const std::string& skeletonFile,
                                                                       const std::string& atlasFile,
                                                                       float scale)
{
    SpineSkeletonAssets assets = loadFromFile(skeletonFile, atlasFile, scale);
    if (!assets.valid())
        return nullptr;
    return SharedSkeletonAnimation::createWithAssets(assets);
}

NS_MG_END

#endif
