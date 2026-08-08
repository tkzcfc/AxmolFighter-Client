#pragma once

#include "mugen/core/StdC.h"
#include "RenderLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "ManualSkeletonAnimation.h"
#    include "mugen/avatar/data/MotionMap.h"

NS_MG_BEGIN

struct SpineAvatarDesc
{
    std::string skeleton;
    std::string atlas;
    std::string motionFile;
    std::string defaultSkin;
    float scale = 1.0f;
};

class SpineLayer : public RenderLayer
{
public:
    static SpineLayer* create(const SpineAvatarDesc& desc);

    bool setMotion(const std::string& motionName, const std::string& entryId, bool loop) override;
    void step(int dtMs) override;
    void seek(int timeMs) override;
    int durationMs() const override;

    int currentTimeMs() const override { return m_timeMs; }

    bool setSkin(const std::string& skinName);

private:
    bool initWithDesc(const SpineAvatarDesc& desc);
    void initMotionMap(const SpineAvatarDesc& desc);
    bool initSkeleton(const SpineAvatarDesc& desc);
    std::string resolveSpineAnim(const std::string& motionName, const std::string& entryId) const;
    void applyTrackTime(int timeMs);

    std::shared_ptr<const MotionMap> m_motionMap;
    ManualSkeletonAnimation* m_skeleton = nullptr;
    int m_timeMs                        = 0;
    float m_spineDurationSec            = 0.0f;
    bool m_loop                         = false;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
