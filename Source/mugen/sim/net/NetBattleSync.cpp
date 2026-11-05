#include "NetBattleSync.h"

#include <utility>

namespace mugen::sim::net
{

NetBattleSync::NetBattleSync(uint32_t localSlot, uint32_t slotCount, NetBattleSyncConfig config)
    : m_config(config)
    , m_localSlot(localSlot)
    , m_session(localSlot, slotCount, config.m_rollbackDepth)
{
}

void NetBattleSync::start(World world, uint64_t startServerTimeMs, uint64_t localNowMs)
{
    m_startServerTimeMs = startServerTimeMs;
    m_session.setWorld(std::move(world));
    m_session.setLocalSlot(m_localSlot);
    m_hasStart          = true;
    m_lastSentHashFrame = 0;
    m_pendingInputs.clear();
    m_pingMs   = 0;
    m_hasPing  = false;
    m_clockOffsetMs = 0;
    m_hasOffset = false;
    updateClockOffset(startServerTimeMs, localNowMs);
}

void NetBattleSync::updateClockOffset(uint64_t serverTimeMs, uint64_t localNowMs)
{
    const int64_t sample = static_cast<int64_t>(serverTimeMs) - static_cast<int64_t>(localNowMs);
    if (!m_hasOffset)
    {
        m_clockOffsetMs = sample;
        m_hasOffset     = true;
    }
    else
    {
        m_clockOffsetMs += (sample - m_clockOffsetMs) / m_config.m_clockSmoothingDiv;
    }
}

void NetBattleSync::onServerClock(uint64_t /*serverFrame*/, uint64_t serverTimeMs, uint64_t localNowMs)
{
    updateClockOffset(serverTimeMs, localNowMs);
    // 单向延迟估计：预测领先帧数扣除固定输入延迟后的剩余帧 × 每帧毫秒。
    const int64_t extra  = static_cast<int64_t>(leadFrames()) - static_cast<int64_t>(m_config.m_inputDelayFrames);
    const int64_t sample = (extra > 0 ? extra : 0) * static_cast<int64_t>(m_config.m_frameMs);
    if (!m_hasPing)
    {
        m_pingMs  = sample;
        m_hasPing = true;
    }
    else
    {
        m_pingMs += (sample - m_pingMs) / m_config.m_clockSmoothingDiv;
    }
}

uint64_t NetBattleSync::estimatedServerNowMs(uint64_t localNowMs) const
{
    const int64_t estimated = static_cast<int64_t>(localNowMs) + m_clockOffsetMs;
    return estimated > 0 ? static_cast<uint64_t>(estimated) : 0;
}

uint64_t NetBattleSync::estimatedServerFrame(uint64_t localNowMs) const
{
    if (!m_hasStart || m_startServerTimeMs == 0)
        return 0;
    const uint64_t now = estimatedServerNowMs(localNowMs);
    if (now <= m_startServerTimeMs)
        return 0;
    return (now - m_startServerTimeMs) / m_config.m_frameMs;
}

uint64_t NetBattleSync::targetFrame(uint64_t localNowMs) const
{
    uint64_t target = estimatedServerFrame(localNowMs) + m_config.m_inputDelayFrames;
    // 预测不得超过回滚窗口，提前封顶避免会话 tick 因领先过多而停顿。
    const uint64_t maxPredict = m_session.confirmedFrame() + m_config.m_rollbackDepth;
    if (target > maxPredict)
        target = maxPredict;
    return target;
}

void NetBattleSync::submitLocalInput(uint64_t frame, PlayerInput input)
{
    input.slot = m_localSlot;
    m_session.addLocalInput(frame, input);
    // 停顿重试时同一帧可能重复提交，覆盖环内最近一条即可避免冗余重复。
    if (!m_pendingInputs.empty() && m_pendingInputs.back().first == frame)
        m_pendingInputs.back().second = std::move(input);
    else
        m_pendingInputs.emplace_back(frame, std::move(input));
    while (m_pendingInputs.size() > m_config.m_redundantInputFrames)
        m_pendingInputs.pop_front();
}

void NetBattleSync::confirmInput(uint64_t frame, uint32_t slot, PlayerInput input)
{
    m_session.addConfirmedInput(frame, slot, std::move(input));
}

void NetBattleSync::confirmSystem(uint64_t frame, SystemCommand command)
{
    m_session.addConfirmedSystem(frame, std::move(command));
}

bool NetBattleSync::checkServerHash(uint64_t frame, uint64_t hash)
{
    const uint64_t mine = m_session.hashAt(frame);
    if (mine == 0)
        return false;  // 快照已被覆盖，无法对账。
    return mine != hash;
}

void NetBattleSync::collectClientHashes(std::vector<std::pair<uint64_t, uint64_t>>& out)
{
    const uint64_t confirmed = m_session.confirmedFrame();
    const uint64_t first = (m_lastSentHashFrame / m_config.m_hashInterval + 1) * m_config.m_hashInterval;
    for (uint64_t frame = first; frame <= confirmed; frame += m_config.m_hashInterval)
    {
        const uint64_t hash = m_session.hashAt(frame);
        if (hash != 0)
            out.emplace_back(frame, hash);
        m_lastSentHashFrame = frame;
    }
}

void NetBattleSync::collectPendingInputs(std::vector<std::pair<uint64_t, PlayerInput>>& out)
{
    const uint64_t confirmed = m_session.confirmedFrame();
    for (const auto& entry : m_pendingInputs)
        if (entry.first > confirmed)
            out.push_back(entry);
}

bool NetBattleSync::resync(uint64_t frame, ByteBuffer& snapshot)
{
    return m_session.resync(frame, snapshot);
}

void NetBattleSync::resetClock(uint64_t serverTimeMs, uint64_t localNowMs)
{
    m_clockOffsetMs = static_cast<int64_t>(serverTimeMs) - static_cast<int64_t>(localNowMs);
    m_hasOffset     = true;
    m_pingMs        = 0;
    m_hasPing       = false;
}

uint64_t NetBattleSync::leadFrames() const
{
    const uint64_t current   = m_session.currentFrame();
    const uint64_t confirmed = m_session.confirmedFrame();
    return current > confirmed ? current - confirmed : 0;
}

}  // namespace mugen::sim::net
