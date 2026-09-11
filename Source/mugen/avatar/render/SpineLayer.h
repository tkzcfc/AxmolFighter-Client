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
    void initMotionMap(const FashionSpineDesc& desc);
    bool initSkeleton(const FashionSpineDesc& desc);
    const MotionEntry* findEntry(const std::string& motionName, const std::string& entryId) const;
    void applyTrackTime(int timeMs);

    std::shared_ptr<const MotionMap> m_motionMap;
    MgSkeletonAnimation* m_skeleton = nullptr;
    int m_timeMs                    = 0;
    int m_durationMs                = 0;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
