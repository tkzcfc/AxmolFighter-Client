#include "MotionLayer.h"

#include "mugen/avatar/data/AvatarAssetCache.h"

NS_MG_BEGIN

bool MotionLayer::init(const AvatarLayerDef& def)
{
    m_def       = def;
    m_motion    = nullptr;
    m_startMs   = 0;
    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(m_def.motionMapPath);
    return m_motionMap != nullptr;
}

bool MotionLayer::setMotion(const std::string& motionName, const std::string& entryId)
{
    m_motion  = nullptr;
    m_startMs = 0;
    if (!m_motionMap)
        return false;

    const Motion* motion = m_motionMap->findMotion(motionName);
    if (!motion || motion->clips.empty())
        return false;

    const int startMs = motion->startTimeMs(entryId);
    if (startMs < 0)
        return false;

    m_motion  = motion;
    m_startMs = startMs;
    return motion->durationMs() > 0;
}

void MotionLayer::clearBox()
{
    m_motion  = nullptr;
    m_startMs = 0;
}

int MotionLayer::durationMs() const
{
    return m_motion ? m_motion->durationMs() : 0;
}

void MotionLayer::boxesAt(int timeMs,
                          std::vector<const DamageBox*>& outAttack,
                          std::vector<const DamageBox*>& outDamage) const
{
    if (!m_motion)
        return;
    m_motion->boxesAt(timeMs, outAttack, outDamage);
}

void MotionLayer::eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const
{
    if (!m_motion)
        return;
    m_motion->eventsBetween(t0, t1, out);
}

NS_MG_END
