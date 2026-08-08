#include "MotionLayer.h"

#include "mugen/avatar/AvatarLayerUtils.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{
bool tryLoadAutoBox(const std::string& baseDir,
                    const std::string& animName,
                    std::shared_ptr<const CombatTimeline>& outBox)
{
    if (baseDir.empty() || animName.empty())
        return false;
    const std::string autoBox = AvatarLayerUtils::resolveAssetPath(baseDir, animName + ".box");
    if (autoBox.empty())
        return false;
    outBox = AvatarAssetCache::getInstance()->getCombatTimeline(autoBox);
    return outBox != nullptr;
}
}  // namespace

bool MotionLayer::init(const AvatarLayerDef& def)
{
    m_def = def;
    m_box.reset();
    m_motionMap.reset();

    if (m_def.motionMapPath.empty())
        return true;

    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(m_def.motionMapPath);
    if (!m_motionMap)
    {
        MG_LOG_E("MotionLayer::init: failed to load MotionMap '{}'", m_def.motionMapPath);
        return false;
    }
    return true;
}

bool MotionLayer::setMotion(const std::string& motionName, const std::string& entryId)
{
    m_box.reset();
    if (!m_motionMap)
    {
        // 直通：旁路 baseDir/<anim>.box 以提供时长权威
        tryLoadAutoBox(m_def.baseDir, motionName, m_box);
        return true;
    }

    const MotionEntry* entry = nullptr;
    if (entryId.empty())
        entry = m_motionMap->entryAt(motionName, 0);
    else
        entry = m_motionMap->findEntry(motionName, entryId);

    if (!entry)
    {
        // 无 entry 时仍允许直通播放（按 motionName 找 .box）
        MG_LOG_D("MotionLayer: motion/entry not found '{}' / '{}', direct accept", motionName, entryId);
        tryLoadAutoBox(m_def.baseDir, motionName, m_box);
        return true;
    }

    const std::string boxPath = AvatarLayerUtils::resolveAssetPath(m_def.baseDir, entry->getBoxPath());
    if (!boxPath.empty())
    {
        m_box = AvatarAssetCache::getInstance()->getCombatTimeline(boxPath);
        if (!m_box)
            MG_LOG_D("MotionLayer: CombatTimeline unavailable '{}' (layer has no boxes)", boxPath);
    }
    if (!m_box)
        tryLoadAutoBox(m_def.baseDir, motionName, m_box);
    return true;
}

void MotionLayer::clearMotion()
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
