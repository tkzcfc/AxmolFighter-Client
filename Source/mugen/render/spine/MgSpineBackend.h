#pragma once

#include "mugen/render/spine/MgSkeletonAnimation.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class MgSpineBackend
{
public:
    virtual ~MgSpineBackend() = default;

    virtual MgSkeletonData* load(const ax::Data& skelData,
                                 const std::vector<std::string>& atlasFiles,
                                 float scale,
                                 std::string_view skeletonFile) const = 0;
    virtual void dispose(MgSkeletonData& data) const                  = 0;

    // 运行时替换图集：atlasFiles 合并为新 atlas，所有 attachment 按同名 region 重指，旧 atlas 释放。
    // 会就地改写 SkeletonData，仅允许实例私有的数据调用（共享数据会影响所有使用者）。
    virtual bool replaceAtlas(MgSkeletonData& data, const std::vector<std::string>& atlasFiles) const = 0;

    // 三段式异步加载：
    // 1. createAtlasHandle（工作线程、parse 前）：创建合并 atlas，禁止建纹理（纯文本解析）
    virtual void* createAtlasHandle(const std::vector<std::string>& atlasFiles) const = 0;
    //    parse 未接管时释放裸 atlas（PipelineState RAII）
    virtual void disposeAtlasHandle(void* atlasHandle) const = 0;
    // 2. parseWithAtlas（后台线程安全）：纯 AtlasAttachmentLoader 解析，不建渲染对象；
    //    成功则接管 atlasHandle 并返回数据；失败内部释放 atlasHandle 并返回 nullptr
    virtual MgSkeletonData* parseWithAtlas(const ax::Data& skelData,
                                           void* atlasHandle,
                                           float scale,
                                           std::string_view skeletonFile) const = 0;
    // 3. bindAtlasTextures + materialize（主线程）：绑定页纹理（预热后为缓存命中），
    //    为所有 attachment 建 AttachmentVertices 渲染对象，并换上正式渲染 loader
    virtual void bindAtlasTextures(void* atlasHandle) const         = 0;
    virtual void materialize(MgSkeletonData& data) const            = 0;

    virtual MgAnimation findAnimation(const MgSkeletonData& data, const char* name) const = 0;
    virtual int animationCount(const MgSkeletonData& data) const                          = 0;
    virtual MgAnimation animationAt(const MgSkeletonData& data, int index) const          = 0;
    virtual bool hasSkin(const MgSkeletonData& data, const char* name) const              = 0;

    virtual ax::Node* createInner(MgSkeletonData* data) const                                                      = 0;
    virtual bool isInnerValid(ax::Node* inner) const                                                               = 0;
    virtual ax::Rect boundingBox(ax::Node* inner) const                                                            = 0;
    virtual void update(ax::Node* inner, float dt) const                                                           = 0;
    virtual MgTrackEntry setAnimation(ax::Node* inner, int trackIndex, const std::string& name, bool loop) const   = 0;
    virtual MgAnimation findAnimationOnNode(ax::Node* inner, const std::string& name) const                        = 0;
    virtual MgTrackEntry getCurrent(ax::Node* inner, int trackIndex) const                                         = 0;
    virtual void keepCurrentTrackAlive(ax::Node* inner, int trackIndex) const                                      = 0;
    virtual void seekCurrentTrack(ax::Node* inner, int trackIndex, float timeSeconds) const                        = 0;
    virtual void setSkin(ax::Node* inner, const std::string& name) const                                           = 0;
    virtual void setSlotsToSetupPose(ax::Node* inner) const                                                        = 0;
    virtual void setTimeScale(ax::Node* inner, float scale) const                                                  = 0;
    virtual void clearTracks(ax::Node* inner) const                                                                = 0;
    virtual void setCompleteListener(ax::Node* inner, const MgSkeletonAnimation::CompleteListener& listener) const = 0;
    virtual void setUpdateOnlyIfVisible(ax::Node* inner, bool value) const                                         = 0;

    static const MgSpineBackend& axmol();
#    if MG_SPINE_USE_3_4
    static const MgSpineBackend& spine34();
#    endif
    static const MgSpineBackend& of(MgSpineRuntime runtime);
};

inline const MgSpineBackend& MgSpineBackend::of(MgSpineRuntime runtime)
{
#    if MG_SPINE_USE_3_4
    if (runtime == MgSpineRuntime::Spine34)
        return spine34();
#    endif
    (void)runtime;
    return axmol();
}

NS_MG_END

#endif
