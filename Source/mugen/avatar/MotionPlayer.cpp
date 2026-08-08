#include "MotionPlayer.h"

#include <algorithm>

NS_MG_BEGIN

MotionPlayer::MotionPlayer() : m_timeMs(0), m_durationMs(0), m_loop(false), m_playing(false) {}

bool MotionPlayer::addLayer(const AvatarLayerDef& def)
{
    MotionLayer layer;
    if (!layer.init(def))
        return false;

    if (m_playing && !m_motionName.empty())
        layer.setMotion(m_motionName, m_entryId);

    m_layers.push_back(std::move(layer));
    return true;
}

void MotionPlayer::removeLayersByTag(int32_t tag)
{
    m_layers.erase(std::remove_if(m_layers.begin(), m_layers.end(),
                                  [tag](const MotionLayer& layer) { return layer.getTag() == tag; }),
                   m_layers.end());
}

void MotionPlayer::clearLayers()
{
    m_layers.clear();
}

void MotionPlayer::recomputeDuration()
{
    m_durationMs = 0;
    for (const auto& layer : m_layers)
        m_durationMs = std::max(m_durationMs, layer.durationMs());

    // 各层时长不一致时告警（忽略 duration=0 空层）
    int firstNonZero = -1;
    bool mismatch    = false;
    for (const auto& layer : m_layers)
    {
        const int d = layer.durationMs();
        if (d <= 0)
            continue;
        if (firstNonZero < 0)
            firstNonZero = d;
        else if (d != firstNonZero)
            mismatch = true;
    }
    if (mismatch)
    {
        std::string detail;
        for (const auto& layer : m_layers)
        {
            if (!detail.empty())
                detail += ", ";
            detail += "tag=" + std::to_string(layer.getTag()) + " dur=" + std::to_string(layer.durationMs());
        }
        MG_LOG_W(
            "MotionPlayer: layer durations mismatch motion='{}' max={} [{}] (using max; short layers freeze last "
            "frame)",
            m_motionName, m_durationMs, detail);
    }

    if (m_durationMs <= 0)
        MG_LOG_W("MotionPlayer: duration is 0 for motion='{}'", m_motionName);
}

void MotionPlayer::applyMotionToLayers()
{
    for (auto& layer : m_layers)
        layer.setMotion(m_motionName, m_entryId);
}

bool MotionPlayer::play(const std::string& motionName, const std::string& entryId, bool loop)
{
    m_motionName = motionName;
    m_entryId    = entryId;
    m_loop       = loop;
    m_timeMs     = 0;
    m_durationMs = 0;

    if (m_layers.empty())
    {
        m_playing = false;
        return false;
    }

    bool anyOk = false;
    for (auto& layer : m_layers)
    {
        if (layer.setMotion(motionName, entryId))
            anyOk = true;
    }

    if (!anyOk)
    {
        MG_LOG_E("MotionPlayer::play: all layers failed motion='{}' entry='{}'", motionName, entryId);
        m_playing    = false;
        m_durationMs = 0;
        m_timeMs     = 0;
        for (auto& layer : m_layers)
            layer.clearMotion();
        return false;
    }

    recomputeDuration();
    m_playing = true;
    return true;
}

void MotionPlayer::setDurationMs(int durationMs)
{
    m_durationMs = std::max(0, durationMs);
    if (m_playing && m_durationMs > 0 && m_timeMs > m_durationMs)
        m_timeMs = m_durationMs;
}

void MotionPlayer::collectEventsRange(int t0, int t1, std::vector<const CombatEvent*>* out) const
{
    if (!out || t1 <= t0)
        return;

    for (const auto& layer : m_layers)
        layer.eventsBetween(t0, t1, *out);
}

void MotionPlayer::sortEventsByTime(std::vector<const CombatEvent*>& events)
{
    std::stable_sort(events.begin(), events.end(), [](const CombatEvent* a, const CombatEvent* b) {
        if (!a || !b)
            return a != nullptr;
        return a->getTimeMs() < b->getTimeMs();
    });
}

void MotionPlayer::step(int dtMs, std::vector<const CombatEvent*>* outEvents)
{
    if (outEvents)
        outEvents->clear();

    if (!m_playing || dtMs <= 0 || m_durationMs <= 0)
    {
        if (m_playing && m_durationMs <= 0)
            m_timeMs = 0;
        return;
    }

    const int oldTime = m_timeMs;
    const int target  = oldTime + dtMs;

    if (!m_loop)
    {
        const int newTime = std::min(target, m_durationMs);
        collectEventsRange(oldTime, newTime, outEvents);
        if (outEvents)
            sortEventsByTime(*outEvents);
        m_timeMs = newTime;
        return;
    }

    // 循环时一步跨多圈最多只收集一整圈事件
    if (dtMs >= m_durationMs)
        MG_LOG_W("MotionPlayer::step: dtMs={} >= duration={}, collect at most one cycle", dtMs, m_durationMs);

    std::vector<const CombatEvent*> firstSeg;
    std::vector<const CombatEvent*> secondSeg;

    if (target < m_durationMs)
    {
        collectEventsRange(oldTime, target, outEvents ? &firstSeg : nullptr);
        m_timeMs = target;
    }
    else
    {
        collectEventsRange(oldTime, m_durationMs, outEvents ? &firstSeg : nullptr);
        const int wrapped = target % m_durationMs;
        collectEventsRange(0, wrapped, outEvents ? &secondSeg : nullptr);
        m_timeMs = wrapped;
    }

    if (outEvents)
    {
        outEvents->insert(outEvents->end(), firstSeg.begin(), firstSeg.end());
        sortEventsByTime(*outEvents);
        // 回绕段排在后
        sortEventsByTime(secondSeg);
        outEvents->insert(outEvents->end(), secondSeg.begin(), secondSeg.end());
    }
}

void MotionPlayer::seek(int timeMs)
{
    if (timeMs < 0)
        timeMs = 0;

    if (m_durationMs <= 0)
    {
        m_timeMs = 0;
        return;
    }

    if (m_loop)
    {
        m_timeMs = timeMs % m_durationMs;
        if (m_timeMs < 0)
            m_timeMs += m_durationMs;
    }
    else
    {
        m_timeMs = std::min(timeMs, m_durationMs);
    }
}

void MotionPlayer::stop()
{
    m_motionName.clear();
    m_entryId.clear();
    m_timeMs     = 0;
    m_durationMs = 0;
    m_loop       = false;
    m_playing    = false;
    for (auto& layer : m_layers)
        layer.clearMotion();
}

bool MotionPlayer::isFinished() const
{
    if (m_loop)
        return false;
    if (!m_playing)
        return true;
    if (m_durationMs <= 0)
        return true;
    return m_timeMs >= m_durationMs;
}

void MotionPlayer::boxesAt(std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const
{
    outAttack.clear();
    outDamage.clear();

    int t = m_timeMs;
    if (m_loop && m_durationMs > 0)
    {
        t %= m_durationMs;
        if (t < 0)
            t += m_durationMs;
    }

    for (const auto& layer : m_layers)
        layer.boxesAt(t, outAttack, outDamage);
}

void MotionPlayer::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    std::vector<AvatarLayerDef> defs;
    defs.reserve(m_layers.size());
    for (const auto& layer : m_layers)
        defs.push_back(layer.getDef());
    byteBuffer.writeValue(defs);
}

bool MotionPlayer::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    std::vector<AvatarLayerDef> defs;
    if (!byteBuffer.getValue(defs))
    {
        MG_LOG_E("MotionPlayer::deserializeCustomImpl: failed to read layer defs");
        return false;
    }

    const std::string motionName = m_motionName;
    const std::string entryId    = m_entryId;
    const int timeMs             = m_timeMs;
    const bool loop              = m_loop;
    const bool playing           = m_playing;
    const auto durationMs        = m_durationMs;

    clearLayers();
    for (const auto& def : defs)
    {
        MotionLayer layer;
        if (!layer.init(def))
        {
            MG_LOG_E("MotionPlayer::deserializeCustomImpl: failed to init layer motion='{}'", def.motionMapPath);
            return false;
        }
        m_layers.push_back(std::move(layer));
    }

    m_motionName = motionName;
    m_entryId    = entryId;
    m_loop       = loop;
    m_playing    = playing;
    m_timeMs     = 0;
    m_durationMs = 0;

    if (!m_motionName.empty())
    {
        applyMotionToLayers();
        recomputeDuration();
        seek(timeMs);
    }
    else
    {
        m_timeMs = timeMs;
    }

    MG_ASSERT(m_durationMs == durationMs && m_timeMs == timeMs);

    m_playing = playing;
    return true;
}

NS_MG_END
