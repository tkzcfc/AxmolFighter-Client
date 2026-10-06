#include "MotionPlayer.h"

#include "mugen/avatar/data/AvatarAssetCache.h"

#include <algorithm>

NS_MG_BEGIN

MotionPlayer::MotionPlayer() : m_timeMs(0), m_loop(false), m_playing(false) {}

bool MotionPlayer::bind(const std::string& motionFile)
{
    stop();
    m_motionFile = motionFile;
    m_motionMap  = motionFile.empty() ? nullptr : AvatarAssetCache::getInstance()->getMotionMap(motionFile);
    if (!m_motionMap)
    {
        MG_LOG_W("MotionPlayer::bind: motion map not found '{}'", motionFile);
        m_motionFile.clear();
        return false;
    }
    return true;
}

void MotionPlayer::unbind()
{
    stop();
    m_motionFile.clear();
    m_motionMap = nullptr;
}

bool MotionPlayer::play(const std::string& motionName, const std::string& entryId, bool loop)
{
    m_motionName = motionName;
    m_entryId    = entryId;
    m_loop       = loop;
    m_timeMs     = 0;
    m_motion     = nullptr;
    m_playing    = false;

    if (!m_motionMap)
        return false;

    const Motion* motion = m_motionMap->findMotion(motionName);
    if (!motion)
    {
        MG_LOG_E("MotionPlayer::play: motion not found '{}' in '{}'", motionName, m_motionFile);
        return false;
    }
    const int startMs = motion->startTimeMs(entryId);
    if (startMs < 0)
    {
        MG_LOG_E("MotionPlayer::play: entry not found motion='{}' entry='{}'", motionName, entryId);
        return false;
    }

    m_motion  = motion;
    m_timeMs  = startMs;
    m_playing = true;
    return true;
}

std::string MotionPlayer::motionNameAt(size_t index) const
{
    if (!m_motionMap)
        return {};
    const Motion* motion = m_motionMap->motionAt(index);
    return motion ? motion->name : std::string();
}

void MotionPlayer::collectEventsRange(int t0, int t1, std::vector<const CombatEvent*>* out) const
{
    if (!out || t1 <= t0)
        return;
    m_motion->eventsBetween(t0, t1, *out);
}

void MotionPlayer::sortEventsByTime(std::vector<const CombatEvent*>& events)
{
    std::stable_sort(events.begin(), events.end(),
                     [](const CombatEvent* a, const CombatEvent* b) { return a->timeMs < b->timeMs; });
}

void MotionPlayer::step(int dtMs, std::vector<const CombatEvent*>* outEvents)
{
    if (outEvents)
        outEvents->clear();

    if (!m_playing || !m_motion || dtMs <= 0)
        return;

    const int duration = m_motion->durationMs();
    const int oldTime  = m_timeMs;
    const int target   = oldTime + dtMs;

    if (!m_loop)
    {
        const int newTime = std::min(target, duration);
        collectEventsRange(oldTime, newTime, outEvents);
        if (outEvents)
            sortEventsByTime(*outEvents);
        m_timeMs = newTime;
        return;
    }

    // 循环时一步跨多圈最多只收集一整圈事件
    if (dtMs >= duration)
        MG_LOG_W("MotionPlayer::step: dtMs={} >= duration={}, collect at most one cycle", dtMs, duration);

    std::vector<const CombatEvent*> secondSeg;
    if (target < duration)
    {
        collectEventsRange(oldTime, target, outEvents);
        m_timeMs = target;
    }
    else
    {
        collectEventsRange(oldTime, duration, outEvents);
        const int wrapped = target % duration;
        collectEventsRange(0, wrapped, outEvents ? &secondSeg : nullptr);
        m_timeMs = wrapped;
    }

    if (outEvents)
    {
        sortEventsByTime(*outEvents);
        // 回绕段排在后
        sortEventsByTime(secondSeg);
        outEvents->insert(outEvents->end(), secondSeg.begin(), secondSeg.end());
    }
}

void MotionPlayer::seek(int timeMs)
{
    if (!m_motion)
    {
        m_timeMs = 0;
        return;
    }

    const int duration = m_motion->durationMs();
    timeMs             = std::max(0, timeMs);
    m_timeMs           = m_loop ? timeMs % duration : std::min(timeMs, duration);
}

void MotionPlayer::stop()
{
    m_motionName.clear();
    m_entryId.clear();
    m_timeMs  = 0;
    m_loop    = false;
    m_playing = false;
    m_motion  = nullptr;
}

bool MotionPlayer::isFinished() const
{
    if (m_loop)
        return false;
    if (!m_playing || !m_motion)
        return true;
    return m_timeMs >= m_motion->durationMs();
}

void MotionPlayer::boxesAt(std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const
{
    outAttack.clear();
    outDamage.clear();
    if (m_motion)
        m_motion->boxesAt(m_timeMs, outAttack, outDamage);
}

bool MotionPlayer::deserializeCustomImpl(ByteBuffer&)
{
    m_motionMap = nullptr;
    m_motion    = nullptr;
    if (m_motionFile.empty())
        return true;

    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(m_motionFile);
    if (!m_motionMap)
    {
        MG_LOG_E("MotionPlayer::deserialize: motion map not found '{}'", m_motionFile);
        return false;
    }
    if (m_motionName.empty())
        return true;

    m_motion = m_motionMap->findMotion(m_motionName);
    if (!m_motion && m_playing)
    {
        MG_LOG_E("MotionPlayer::deserialize: motion not found '{}' in '{}'", m_motionName, m_motionFile);
        return false;
    }
    return true;
}

NS_MG_END
