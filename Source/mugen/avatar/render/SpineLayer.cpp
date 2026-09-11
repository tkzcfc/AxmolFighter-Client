#include "SpineLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/data/AvatarAssetCache.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"

#    include <algorithm>
#    include <cmath>

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
    const float scale    = desc.scale > 0.0f ? desc.scale : 1.0f;
    auto* cache          = SpineSkeletonCache::getInstance();
    MgSkeletonData* data = cache->getOrCreate(desc.skeleton, desc.atlases, scale);
    m_skeleton           = data ? MgSkeletonAnimation::createWithData(data) : nullptr;
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

bool SpineLayer::setMotion(const std::string& motionName, const std::string& entryId)
{
    m_timeMs    = 0;
    m_clipIndex = static_cast<size_t>(-1);
    m_motion    = nullptr;

    if (!m_skeleton || !m_motionMap)
        return false;

    const Motion* motion = m_motionMap->findMotion(motionName);
    if (!motion || motion->clips.empty())
        return false;

    const int startMs = motion->startTimeMs(entryId);
    if (startMs < 0)
        return false;

    for (size_t i = 0; i < motion->clipCount(); ++i)
    {
        const MotionClip* clip = motion->clipAtIndex(i);
        if (clip->type != MotionEntryType::kSpine || clip->source.empty())
            return false;

        MgAnimation anim = m_skeleton->findAnimation(clip->source);
        if (!anim)
        {
            MG_LOG_W("SpineLayer: animation not found '{}'", clip->source);
            return false;
        }

        MG_ASSERT(clip->durationMs == static_cast<int>(std::lround(anim.duration() * 1000.0f)));
    }

    m_motion = motion;
    m_timeMs = startMs;
    applyTrackTime(m_timeMs);
    return true;
}

int SpineLayer::durationMs() const
{
    return m_motion ? m_motion->durationMs() : 0;
}

void SpineLayer::applyTrackTime(int timeMs)
{
    if (!m_skeleton || !m_motion)
        return;

    int localMs            = 0;
    size_t index           = 0;
    const MotionClip* clip = m_motion->clipAt(timeMs, &localMs, &index);
    if (!clip)
        return;

    if (index != m_clipIndex)
    {
        const std::string& spineAnim = clip->source;
        if (!m_skeleton->setAnimation(0, spineAnim, false))
        {
            MG_LOG_W("SpineLayer: setAnimation failed '{}'", spineAnim);
            return;
        }
        // setAnimation(..., false) 会新建一条 不循环 的 TrackEntry。
        // Spine 默认把 trackEnd / endTime 设成动画时长，时间一到就 complete，然后丢掉这条 track。
        // 丢掉之后 getCurrent(0) 为空，姿势会掉回 setup pose
        // 所以这里要 keepCurrentTrackAlive(0) 保持这条 track 不被丢掉
        // 他的实现是把 trackEnd / endTime 设成 FLT_MAX，时间永远到不了
        // 这样 seek 到末尾、update(0)、或 update(dt) 稍稍越过动画时长，track 都还在，最后一帧能freeze住，下一次
        // seekCurrentTrack 也还有当前 entry
        m_skeleton->keepCurrentTrackAlive(0);
        m_clipIndex = index;
    }

    float tSec = static_cast<float>(std::max(0, localMs)) / 1000.0f;
    if (clip->durationMs > 0)
        tSec = std::min(tSec, static_cast<float>(clip->durationMs) / 1000.0f);

    if (m_skeleton->getCurrent(0))
    {
        // 跳转到指定时间点
        m_skeleton->seekCurrentTrack(0, tSec);
    }
    m_skeleton->update(0.0f);
}

void SpineLayer::step(int dtMs)
{
    if (dtMs <= 0 || !m_skeleton || !m_motion)
        return;

    const int dur = durationMs();
    if (dur > 0 && m_timeMs >= dur)
        return;

    const size_t oldIndex = m_clipIndex;
    int applyMs           = dtMs;
    m_timeMs += dtMs;
    if (dur > 0 && m_timeMs > dur)
    {
        applyMs -= (m_timeMs - dur);
        m_timeMs = dur;
    }

    size_t newIndex = oldIndex;
    int localMs     = 0;
    m_motion->clipAt(m_timeMs, &localMs, &newIndex);
    if (newIndex != oldIndex)
        applyTrackTime(m_timeMs);
    else if (applyMs > 0)
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
