#include "MotionMap.h"

#include "mugen/core/StdC.h"

#include <algorithm>
#include <cstdio>

NS_MG_BEGIN

int Motion::startTimeMs(const std::string& entryId) const
{
    if (entryId.empty())
        return 0;
    for (const MotionClip& clip : clips)
    {
        if (clip.id == entryId)
            return clip.startMs;
    }
    return -1;
}

const MotionClip* Motion::clipAtIndex(size_t index) const
{
    if (index >= clips.size())
        return nullptr;
    return &clips[index];
}

const MotionClip* Motion::clipAt(int timeMs, int* localMs, size_t* index) const
{
    if (clips.empty())
        return nullptr;

    int t = std::max(0, timeMs);
    if (t >= duration)
    {
        const size_t last = clips.size() - 1;
        if (localMs)
            *localMs = clips[last].durationMs;
        if (index)
            *index = last;
        return &clips[last];
    }

    for (size_t i = 0; i < clips.size(); ++i)
    {
        const MotionClip& clip = clips[i];
        const int endMs        = clip.startMs + clip.durationMs;
        if (t < endMs || i + 1 == clips.size())
        {
            if (localMs)
                *localMs = t - clip.startMs;
            if (index)
                *index = i;
            return &clip;
        }
    }
    return nullptr;
}

void Motion::boxesAt(int timeMs,
                     std::vector<const DamageBox*>& outAttack,
                     std::vector<const DamageBox*>& outDamage) const
{
    int localMs            = 0;
    const MotionClip* clip = clipAt(timeMs, &localMs);
    if (!clip)
        return;
    clip->timeline->boxesAt(localMs, outAttack, outDamage);
}

void Motion::eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const
{
    if (t1 <= t0 || clips.empty() || t0 >= duration)
        return;

    const int t1Clamped = std::min(t1, duration);
    if (t1Clamped <= t0)
        return;

    for (const MotionClip& clip : clips)
    {
        const int clipEnd = clip.startMs + clip.durationMs;
        if (clipEnd <= t0)
            continue;
        if (clip.startMs >= t1Clamped)
            break;
        const int local0 = std::max(0, t0 - clip.startMs);
        const int local1 = std::min(clip.durationMs, t1Clamped - clip.startMs);
        if (local1 > local0)
            clip.timeline->eventsBetween(local0, local1, out);
    }
}

bool MotionMap::bindTimelines(const std::function<const CombatTimeline*(const std::string&)>& lookup)
{
    m_motions.clear();
    m_nameToIndex.clear();
    m_motions.reserve(defs.size());

    for (const MotionDef& def : defs)
    {
        if (def.name.empty() || def.entries.empty())
        {
            MG_LOG_E("MotionMap: empty motion name/entries in '{}'", sourcePath);
            std::fprintf(stderr, "MotionMap: empty motion name/entries in '%s'\n", sourcePath.c_str());
            return false;
        }
        if (m_nameToIndex.find(def.name) != m_nameToIndex.end())
        {
            MG_LOG_E("MotionMap: duplicate motion '{}' in '{}'", def.name, sourcePath);
            std::fprintf(stderr, "MotionMap: duplicate motion '%s' in '%s'\n", def.name.c_str(), sourcePath.c_str());
            return false;
        }

        Motion motion;
        motion.name = def.name;
        motion.clips.reserve(def.entries.size());
        int cursor = 0;
        for (const MotionEntry& entry : def.entries)
        {
            if (entry.boxPath.empty())
            {
                MG_LOG_E("MotionMap: empty boxPath motion='{}' entry='{}' in '{}'", def.name, entry.id, sourcePath);
                std::fprintf(stderr, "MotionMap: empty boxPath motion='%s' entry='%s' in '%s'\n", def.name.c_str(),
                             entry.id.c_str(), sourcePath.c_str());
                return false;
            }
            const CombatTimeline* timeline = lookup(entry.boxPath);
            if (!timeline)
            {
                MG_LOG_E("MotionMap: missing box '{}' motion='{}' in '{}'", entry.boxPath, def.name, sourcePath);
                std::fprintf(stderr, "MotionMap: missing box '%s' motion='%s' in '%s'\n", entry.boxPath.c_str(),
                             def.name.c_str(), sourcePath.c_str());
                return false;
            }

            MotionClip clip;
            clip.id         = entry.id;
            clip.type       = entry.type;
            clip.source     = entry.source;
            clip.timeline   = timeline;
            clip.startMs    = cursor;
            clip.durationMs = timeline->duration;
            cursor += clip.durationMs;
            motion.clips.push_back(std::move(clip));
        }

        motion.duration = cursor;
        if (motion.duration <= 0)
        {
            MG_LOG_E("MotionMap: duration<=0 motion='{}' in '{}'", def.name, sourcePath);
            std::fprintf(stderr, "MotionMap: duration<=0 motion='%s' in '%s'\n", def.name.c_str(), sourcePath.c_str());
            return false;
        }
        m_nameToIndex[motion.name] = m_motions.size();
        m_motions.push_back(std::move(motion));
    }
    return true;
}

const Motion* MotionMap::findMotion(const std::string& name) const
{
    const auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end())
        return nullptr;
    return &m_motions[it->second];
}

const Motion* MotionMap::motionAt(size_t index) const
{
    if (index >= m_motions.size())
        return nullptr;
    return &m_motions[index];
}

NS_MG_END
