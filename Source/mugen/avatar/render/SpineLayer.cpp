#include "SpineLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/data/AvatarAssetCache.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"

NS_MG_BEGIN

SpineLayer* SpineLayer::create(const FashionSpineDesc& desc)
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

bool SpineLayer::initWithDesc(const FashionSpineDesc& desc)
{
    if (!ax::Node::init())
    {
        MG_LOG_E("SpineLayer::init: invalid args");
        return false;
    }

    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(desc.motionFile);
    if (!m_motionMap)
    {
        MG_LOG_E("SpineLayer::init: failed to load motion map '{}'", desc.motionFile);
        return false;
    }

    if (!initSkeleton(desc))
        return false;

    if (!desc.skin.empty())
        setSkin(desc.skin);

    return true;
}

bool SpineLayer::initSkeleton(const FashionSpineDesc& desc)
{
    const float scale = desc.scale > 0.0f ? desc.scale : 1.0f;
    auto* cache       = SpineSkeletonCache::getInstance();
    MgSkeletonData* data = cache->getOrCreate(desc.skeleton, desc.atlases, scale);
    m_skeleton = data ? MgSkeletonAnimation::createWithData(data) : nullptr;
    if (!m_skeleton)
    {
        MG_LOG_E("SpineLayer::init: failed to load skeleton '{}' atlas '{}'", desc.skeleton, desc.atlases.front());
        return false;
    }

    m_skeleton->setAutoUpdate(false);
    m_skeleton->setUpdateOnlyIfVisible(false);
    m_skeleton->setTimeScale(1.0f);
    addChild(m_skeleton);
    return true;
}

ax::Rect SpineLayer::skeletonBoundingBox() const
{
    if (!m_skeleton)
        return ax::Rect::ZERO;
    return m_skeleton->getBoundingBox();
}

bool SpineLayer::setSkin(const std::string& skinName)
{
    if (!m_skeleton || skinName.empty())
        return false;
    if (!m_skeleton->skeletonData()->hasSkin(skinName.c_str()))
    {
        MG_LOG_W("SpineLayer: skin not found '{}'", skinName);
        return false;
    }
    m_skeleton->setSkin(skinName);
    m_skeleton->setSlotsToSetupPose();
    return true;
}

const MotionEntry* SpineLayer::findEntry(const std::string& motionName, const std::string& entryId) const
{
    if (!m_motionMap)
        return nullptr;
    if (entryId.empty())
        return m_motionMap->entryAt(motionName, 0);
    return m_motionMap->findEntry(motionName, entryId);
}

bool SpineLayer::setMotion(const std::string& motionName, const std::string& entryId)
{
    m_timeMs     = 0;
    m_durationMs = 0;

    if (!m_skeleton)
        return false;

    const MotionEntry* entry = findEntry(motionName, entryId);
    if (!entry || entry->getType() != MotionEntryType::kSpine || entry->getSource().empty())
        return false;

    const auto box = AvatarAssetCache::getInstance()->getCombatTimeline(entry->getBoxPath());
    if (!box)
        return false;
    m_durationMs = box->getDuration();

    const std::string& spineAnim = entry->getSource();
    MgAnimation anim             = m_skeleton->findAnimation(spineAnim);
    if (!anim)
    {
        MG_LOG_W("SpineLayer: animation not found '{}'", spineAnim);
        return false;
    }

    MG_ASSERT(m_durationMs == static_cast<int>(std::lround(anim.duration() * 1000.0f)));

    if (!m_skeleton->setAnimation(0, spineAnim, false))
    {
        MG_LOG_W("SpineLayer: setAnimation failed '{}'", spineAnim);
        return false;
    }
    m_skeleton->keepCurrentTrackAlive(0);

    applyTrackTime(0);
    return true;
}

int SpineLayer::durationMs() const
{
    return m_durationMs;
}

void SpineLayer::applyTrackTime(int timeMs)
{
    if (!m_skeleton)
        return;

    const int tMs = std::max(0, timeMs);
    float tSec    = static_cast<float>(tMs) / 1000.0f;
    if (m_durationMs > 0)
        tSec = std::min(tSec, static_cast<float>(m_durationMs) / 1000.0f);

    if (m_skeleton->getCurrent(0))
        m_skeleton->seekCurrentTrack(0, tSec);
    m_skeleton->update(0.0f);
}

void SpineLayer::step(int dtMs)
{
    if (dtMs <= 0 || !m_skeleton)
        return;

    const int dur = durationMs();
    if (dur > 0 && m_timeMs >= dur)
        return;

    int applyMs = dtMs;
    m_timeMs += dtMs;
    if (dur > 0 && m_timeMs > dur)
    {
        applyMs -= (m_timeMs - dur);
        m_timeMs = dur;
    }
    if (applyMs > 0)
        m_skeleton->update(static_cast<float>(applyMs) / 1000.0f);
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
