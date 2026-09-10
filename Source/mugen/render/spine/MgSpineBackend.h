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

    virtual MgAnimation findAnimation(const MgSkeletonData& data, const char* name) const = 0;
    virtual int animationCount(const MgSkeletonData& data) const                          = 0;
    virtual MgAnimation animationAt(const MgSkeletonData& data, int index) const          = 0;
    virtual bool hasSkin(const MgSkeletonData& data, const char* name) const              = 0;

    virtual ax::Node* createInner(MgSkeletonData* data) const = 0;
    virtual bool isInnerValid(ax::Node* inner) const                                 = 0;
    virtual ax::Rect boundingBox(ax::Node* inner) const                              = 0;
    virtual void update(ax::Node* inner, float dt) const                             = 0;
    virtual MgTrackEntry setAnimation(ax::Node* inner, int trackIndex, const std::string& name, bool loop) const = 0;
    virtual MgAnimation findAnimationOnNode(ax::Node* inner, const std::string& name) const = 0;
    virtual MgTrackEntry getCurrent(ax::Node* inner, int trackIndex) const                  = 0;
    virtual void keepCurrentTrackAlive(ax::Node* inner, int trackIndex) const               = 0;
    virtual void seekCurrentTrack(ax::Node* inner, int trackIndex, float timeSeconds) const = 0;
    virtual void setSkin(ax::Node* inner, const std::string& name) const                    = 0;
    virtual void setSlotsToSetupPose(ax::Node* inner) const                                 = 0;
    virtual void setTimeScale(ax::Node* inner, float scale) const                           = 0;
    virtual void clearTracks(ax::Node* inner) const                                         = 0;
    virtual void setCompleteListener(ax::Node* inner,
                                     const MgSkeletonAnimation::CompleteListener& listener) const = 0;
    virtual void setUpdateOnlyIfVisible(ax::Node* inner, bool value) const                  = 0;

    static const MgSpineBackend& axmol();
#if MG_SPINE_USE_3_4
    static const MgSpineBackend& spine34();
#endif
    static const MgSpineBackend& of(MgSpineRuntime runtime);
};

inline const MgSpineBackend& MgSpineBackend::of(MgSpineRuntime runtime)
{
#if MG_SPINE_USE_3_4
    if (runtime == MgSpineRuntime::Spine34)
        return spine34();
#endif
    (void)runtime;
    return axmol();
}

NS_MG_END

#endif
