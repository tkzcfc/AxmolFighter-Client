#include "MotionLayer.h"

#include "mugen/avatar/data/AvatarAssetCache.h"

#include <algorithm>

NS_MG_BEGIN

bool MotionLayer::init(const AvatarLayerDef& def)
{
    m_def = def;
    m_box.reset();
    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(m_def.motionMapPath);
    return m_motionMap != nullptr;
}

bool MotionLayer::setMotion(const std::string& motionName, const std::string& entryId)
{
    m_box.reset();
    if (!m_motionMap)
        return false;

    const MotionEntry* entry = nullptr;
    if (entryId.empty())
        entry = m_motionMap->entryAt(motionName, 0);
    else
        entry = m_motionMap->findEntry(motionName, entryId);

    if (!entry)
        return false;

    m_box = AvatarAssetCache::getInstance()->getCombatTimeline(entry->getBoxPath());

    return true;
}

void MotionLayer::clearBox()
{
    m_box.reset();
}

int MotionLayer::durationMs() const
{
    return m_box ? m_box->getDuration() : 0;
}

void MotionLayer::boxesAt(int timeMs,
                          std::vector<const DamageBox*>& outAttack,
                          std::vector<const DamageBox*>& outDamage) const
{
    if (!m_box)
        return;
    const int layerDur = durationMs();
    if (layerDur <= 0)
        return;
    // 超过本层时长时冻结末帧采样
    const int sample = std::min(std::max(0, timeMs), layerDur);
    m_box->boxesAt(sample, outAttack, outDamage);
}

void MotionLayer::eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const
{
    if (!m_box)
        return;
    const int layerDur = durationMs();
    if (layerDur <= 0 || t0 >= layerDur)
        return;
    const int t1Clamped = std::min(t1, layerDur);
    if (t1Clamped <= t0)
        return;

    m_box->eventsBetween(t0, t1Clamped, out);
}

NS_MG_END
