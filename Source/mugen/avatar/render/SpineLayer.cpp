#include "SpineLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/data/AvatarAssetCache.h"

#    include <cmath>
#    include <limits>

NS_MG_BEGIN

SpineLayer* SpineLayer::create(const SpineAvatarDesc& desc)
{
    auto* ret = new (std::nothrow) SpineLayer();
    if (ret && ret->initWithDesc(desc))
    {
        ret->autorelease();
        return ret;
    }
    AX_SAFE_DELETE(ret);
    return nullptr;
}

bool SpineLayer::initWithDesc(const SpineAvatarDesc& desc)
{
    if (!ax::Node::init())
    {
        MG_LOG_E("SpineLayer::init: invalid args");
        return false;
    }
    if (desc.skeleton.empty() || desc.atlas.empty())
    {
        MG_LOG_E("SpineLayer::init: spineSkeleton/spineAtlas required");
        return false;
    }

    setLayerTag(kAvatarLayerTagCharacter);
    initMotionMap(desc);
    if (!initSkeleton(desc))
        return false;

    if (!desc.defaultSkin.empty())
        setSkin(desc.defaultSkin);

    return true;
}

void SpineLayer::initMotionMap(const SpineAvatarDesc& desc)
{
    if (desc.motionFile.empty())
        return;

    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(desc.motionFile);
    if (!m_motionMap)
        MG_LOG_W("SpineLayer: failed to load MotionMap '{}'", desc.motionFile);
}

bool SpineLayer::initSkeleton(const SpineAvatarDesc& desc)
{
    const float scale = desc.scale > 0.0f ? desc.scale : 1.0f;
    m_skeleton        = ManualSkeletonAnimation::createWithFile(desc.skeleton, desc.atlas, scale);
    if (!m_skeleton)
    {
        MG_LOG_E("SpineLayer::init: failed to load skeleton '{}' atlas '{}'", desc.skeleton, desc.atlas);
        return false;
    }

    m_skeleton->setUpdateOnlyIfVisible(false);
    m_skeleton->setTimeScale(1.0f);
    addChild(m_skeleton);
    return true;
}

bool SpineLayer::setSkin(const std::string& skinName)
{
    if (!m_skeleton || skinName.empty())
        return false;
    if (!m_skeleton->getSkeleton() || !m_skeleton->getSkeleton()->getData())
        return false;
    spine::Skin* skin = m_skeleton->getSkeleton()->getData()->findSkin(skinName.c_str());
    if (!skin)
    {
        MG_LOG_W("SpineLayer: skin not found '{}'", skinName);
        return false;
    }
    m_skeleton->setSkin(skinName);
    m_skeleton->setSlotsToSetupPose();
    return true;
}

std::string SpineLayer::resolveSpineAnim(const std::string& motionName, const std::string& entryId) const
{
    if (!m_motionMap)
        return motionName;

    const MotionEntry* entry = nullptr;
    if (entryId.empty())
        entry = m_motionMap->entryAt(motionName, 0);
    else
        entry = m_motionMap->findEntry(motionName, entryId);

    if (entry && entry->getType() == MotionEntryType::kSpine && !entry->getSource().empty())
        return entry->getSource();

    return motionName;
}

bool SpineLayer::setMotion(const std::string& motionName, const std::string& entryId, bool loop)
{
    m_loop             = loop;
    m_timeMs           = 0;
    m_spineDurationSec = 0.0f;

    if (!m_skeleton)
        return false;

    const std::string spineAnim = resolveSpineAnim(motionName, entryId);

    if (!m_skeleton->findAnimation(spineAnim))
    {
        MG_LOG_W("SpineLayer: animation not found '{}', empty play", spineAnim);
        return false;
    }

    spine::Animation* anim = m_skeleton->findAnimation(spineAnim);
    m_spineDurationSec     = anim ? anim->getDuration() : 0.0f;

    spine::TrackEntry* track = m_skeleton->setAnimation(0, spineAnim, /*loop=*/false);
    if (!track)
    {
        MG_LOG_W("SpineLayer: setAnimation failed '{}'", spineAnim);
        return false;
    }
    track->setTrackEnd(std::numeric_limits<float>::max());

    applyTrackTime(0);
    return true;
}

int SpineLayer::durationMs() const
{
    if (m_spineDurationSec > 0.0f)
        return static_cast<int>(std::lround(m_spineDurationSec * 1000.0f));
    return 0;
}

void SpineLayer::applyTrackTime(int timeMs)
{
    if (!m_skeleton)
        return;

    const int tMs = std::max(0, timeMs);
    float tSec    = static_cast<float>(tMs) / 1000.0f;
    if (m_spineDurationSec > 0.0f)
        tSec = std::min(tSec, m_spineDurationSec);

    if (spine::TrackEntry* track = m_skeleton->getCurrent(0))
    {
        track->setTrackTime(tSec);
        track->setAnimationLast(tSec);
    }
    m_skeleton->update(0.0f);
}

void SpineLayer::step(int dtMs)
{
    if (dtMs > 0)
        m_timeMs += dtMs;
    applyTrackTime(m_timeMs);
}

void SpineLayer::seek(int timeMs)
{
    if (timeMs < 0)
        timeMs = 0;
    m_timeMs = timeMs;
    applyTrackTime(timeMs);
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
