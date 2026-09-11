#pragma once

#include "mugen/core/StdC.h"
#include "RenderLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSkeletonAnimation.h"
#    include "mugen/avatar/data/MotionMap.h"
#    include "mugen/avatar/FashionSpine.h"

NS_MG_BEGIN

class SpineLayer : public RenderLayer
{
public:
    static SpineLayer* create(const FashionSpineDesc& desc);

    bool setMotion(const std::string& motionName, const std::string& entryId) override;
    void step(int dtMs) override;
    void seek(int timeMs) override;
    int durationMs() const override;

    int currentTimeMs() const override { return m_timeMs; }

    bool setSkin(const std::string& skinName);

    ax::Rect skeletonBoundingBox() const;

private:
    bool initWithDesc(const FashionSpineDesc& desc);
    bool initSkeleton(const FashionSpineDesc& desc);
    void applyTrackTime(int timeMs);

    const MotionMap* m_motionMap    = nullptr;
    const Motion* m_motion          = nullptr;
    MgSkeletonAnimation* m_skeleton = nullptr;
    int m_timeMs                    = 0;
    size_t m_clipIndex              = static_cast<size_t>(-1);
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
