#include "MotionPlayer.h"

#include "mugen/avatar/data/AvatarAssetCache.h"

#include <algorithm>
#include <set>

namespace
{
// 进程级诊断：同一缺失动作路径只记一次，不参与战斗状态。
std::set<std::string> g_loggedMissingMotions;
}  // namespace

NS_MG_BEGIN

// 创建未绑定资源且未播放的播放器。
MotionPlayer::MotionPlayer() : m_timeMs(0), m_loop(false), m_playing(false) {}

// 绑定动作图并重置当前播放状态。
bool MotionPlayer::bind(const std::string& motionFile)
{
    stop();
    m_motionFile = motionFile;
    m_motionMap  = motionFile.empty() ? nullptr : AvatarAssetCache::getInstance()->getMotionMap(motionFile);
    if (!m_motionMap)
    {
        if (!motionFile.empty() && g_loggedMissingMotions.insert(motionFile).second)
            MG_LOG_W("MotionPlayer::bind: motion map not found '{}'", motionFile);
        m_motionFile.clear();
        return false;
    }
    return true;
}

// 解除动作图并清空播放状态。
void MotionPlayer::unbind()
{
    stop();
    m_motionFile.clear();
    m_motionMap = nullptr;
}

// 查找动作和起始片段后开始播放。
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

// 返回动作图中指定索引的动作名称。
std::string MotionPlayer::motionNameAt(size_t index) const
{
    if (!m_motionMap)
        return {};
    const Motion* motion = m_motionMap->motionAt(index);
    return motion ? motion->name : std::string();
}

// 收集当前动作指定时间范围内的事件。
void MotionPlayer::collectEventsRange(int t0, int t1, std::vector<const CombatEvent*>* out) const
{
    if (!out || t1 <= t0)
        return;
    m_motion->eventsBetween(t0, t1, *out);
}

// 稳定排序事件以保留同一时刻的源顺序。
void MotionPlayer::sortEventsByTime(std::vector<const CombatEvent*>& events)
{
    std::stable_sort(events.begin(), events.end(),
                     [](const CombatEvent* a, const CombatEvent* b) { return a->timeMs < b->timeMs; });
}

// 推进播放时间，并按循环规则收集经过的事件。
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

// 将播放时间限制或回绕到有效动作范围。
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

// 停止播放并清除当前动作选择。
void MotionPlayer::stop()
{
    m_motionName.clear();
    m_entryId.clear();
    m_timeMs  = 0;
    m_loop    = false;
    m_playing = false;
    m_motion  = nullptr;
}

// 判断非循环动作是否到达末尾。
bool MotionPlayer::isFinished() const
{
    if (m_loop)
        return false;
    if (!m_playing || !m_motion)
        return true;
    return m_timeMs >= m_motion->durationMs();
}

// 返回当前时刻生效的攻击框和受击框。
void MotionPlayer::boxesAt(std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const
{
    outAttack.clear();
    outDamage.clear();
    if (m_motion)
        m_motion->boxesAt(m_timeMs, outAttack, outDamage);
}

NS_MG_END
