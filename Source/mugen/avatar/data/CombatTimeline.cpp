#include "CombatTimeline.h"

NS_MG_BEGIN

// 在线性有序关键帧中查找不晚于指定时间的项。
const CombatKey* CombatTimeline::keyAtOrBefore(const CombatTrack& track, int timeMs)
{
    const CombatKey* best = nullptr;
    for (const CombatKey& key : track.keys)
    {
        if (key.timeMs > timeMs)
            break;
        best = &key;
    }
    return best;
}

// 按轨道种类输出当前有效框体。
void CombatTimeline::boxesAt(int timeMs,
                             std::vector<const DamageBox*>& outAttack,
                             std::vector<const DamageBox*>& outDamage) const
{
    for (const CombatTrack& track : tracks)
    {
        const CombatKey* key = keyAtOrBefore(track, timeMs);
        if (!key || !key->hasBox)
            continue;

        const DamageBox* box = &key->box;
        if (track.kind == CombatTrackKind::Attack)
        {
            outAttack.push_back(box);
        }
        else if (track.kind == CombatTrackKind::Damage)
        {
            outDamage.push_back(box);
        }
        else
        {
            outAttack.push_back(box);
            outDamage.push_back(box);
        }
    }
}

// 收集区间 [t0, t1) 内的有序事件。
void CombatTimeline::eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const
{
    if (t1 <= t0)
        return;

    for (const CombatEvent& event : events)
    {
        const int t = event.timeMs;
        if (t >= t1)
            break;
        if (t >= t0)
            out.push_back(&event);
    }
}

NS_MG_END
