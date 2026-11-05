#include "NetBattleSync.h"

#include <algorithm>
#include <utility>

namespace mugen::sim::net
{

NetBattleSync::NetBattleSync(uint32_t localSlot, uint32_t slotCount, NetBattleSyncConfig config)
    : m_config(config)
    , m_localSlot(localSlot)
    , m_session(localSlot, slotCount, config.m_rollbackDepth)
{
    m_leadFrames = std::clamp(m_config.m_inputDelayFrames, m_config.m_minLeadFrames, maxLead());
}

void NetBattleSync::start(World world, uint64_t startServerTimeMs, uint64_t localNowMs)
{
    m_startServerTimeMs = startServerTimeMs;
    m_session.setWorld(std::move(world));
    m_session.setLocalSlot(m_localSlot);
    m_hasStart          = true;
    m_lastSentHashFrame = 0;
    m_pendingInputs.clear();
    m_leadFrames = std::clamp(m_config.m_inputDelayFrames, m_config.m_minLeadFrames, maxLead());
    m_leadEffectiveFrame = m_session.currentFrame() + 1;
    resetShrinkWindow();
    m_rttMs    = 0;
    m_rttVarMs = 0;
    m_hasRtt   = false;
    m_lateInputs = 0;
    m_lastSlack  = 0;
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
}

uint32_t NetBattleSync::maxLead() const
{
    // 领先量不得超过回滚窗口（留出重推余量）与服务器接受输入的上限（留出时钟估计误差）。
    const uint32_t byDepth  = m_config.m_rollbackDepth > 16 ? m_config.m_rollbackDepth - 16 : 1;
    const uint32_t byServer = kMaxInputLead > 8 ? kMaxInputLead - 8 : 1;
    return std::max(m_config.m_minLeadFrames, std::min({m_config.m_maxLeadFrames, byDepth, byServer}));
}

uint32_t NetBattleSync::shrinkFloor() const
{
    uint32_t floor = m_config.m_minLeadFrames;
    if (m_hasRtt)
    {
        // 估计时钟落后真实服务器约一个单向时延，输入再花一个单向时延才到：至少需要一个 RTT（含抖动）的领先。
        const int64_t rttHigh = m_rttMs + 2 * m_rttVarMs;
        const auto frameMs    = static_cast<int64_t>(m_config.m_frameMs);
        floor = std::max(floor, static_cast<uint32_t>((rttHigh + frameMs - 1) / frameMs));
    }
    return std::min(floor, maxLead());
}

void NetBattleSync::setLead(uint32_t lead)
{
    lead = std::clamp(lead, m_config.m_minLeadFrames, maxLead());
    if (lead == m_leadFrames)
        return;
    m_leadFrames         = lead;
    m_leadEffectiveFrame = m_session.currentFrame() + 1;
    resetShrinkWindow();
}

void NetBattleSync::resetShrinkWindow()
{
    m_shrinkWindowStart    = 0;
    m_shrinkWindowMinSlack = 0;
}

void NetBattleSync::onServerFeedback(const ServerInputFeedback& feedback, uint64_t localNowMs)
{
    m_lateInputs = feedback.m_lateInputs;

    if (feedback.m_echoClientTimeMs != 0 && localNowMs >= feedback.m_echoClientTimeMs)
    {
        const auto sample = static_cast<int64_t>(localNowMs - feedback.m_echoClientTimeMs);
        if (!m_hasRtt)
        {
            m_rttMs    = sample;
            m_rttVarMs = sample / 2;
            m_hasRtt   = true;
        }
        else
        {
            const int64_t deviation = sample > m_rttMs ? sample - m_rttMs : m_rttMs - sample;
            m_rttVarMs += (deviation - m_rttVarMs) / 4;
            m_rttMs += (sample - m_rttMs) / 8;
        }
    }

    if (feedback.m_slackSamples == 0)
        return;
    m_lastSlack = feedback.m_slackMin;
    // 领先量刚变化时，途中仍有旧领先量下发出的帧，它们的富余不代表新领先量。
    if (feedback.m_slackFrame < m_leadEffectiveFrame)
        return;

    // 增大要快：富余不足目标时一次补足缺口。
    if (feedback.m_slackMin < m_config.m_slackTargetFrames)
    {
        const auto deficit = static_cast<uint32_t>(m_config.m_slackTargetFrames - feedback.m_slackMin);
        setLead(m_leadFrames + std::min(deficit, m_config.m_maxLeadStep));
        return;
    }

    // 缩小要慢：窗口内最小富余始终明显高于目标，才每窗口缩小 1 帧，且不低于 RTT 估计所需。
    if (m_shrinkWindowStart == 0)
    {
        m_shrinkWindowStart    = feedback.m_slackFrame;
        m_shrinkWindowMinSlack = feedback.m_slackMin;
    }
    else
    {
        m_shrinkWindowMinSlack = std::min(m_shrinkWindowMinSlack, feedback.m_slackMin);
    }
    if (feedback.m_slackFrame - m_shrinkWindowStart < m_config.m_shrinkWindowFrames)
        return;
    const int32_t surplus = m_shrinkWindowMinSlack - (m_config.m_slackTargetFrames + m_config.m_shrinkMarginFrames);
    const uint32_t floor  = shrinkFloor();
    if (surplus >= 0 && m_leadFrames > floor)
    {
        // 富余越大缩得越多（约三分之一的富余），但每窗口至少 1 帧、至多缩到下限。
        const uint32_t step = 1 + static_cast<uint32_t>(surplus) / 3;
        setLead(m_leadFrames - std::min(step, m_leadFrames - floor));
    }
    resetShrinkWindow();
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
    uint64_t target = estimatedServerFrame(localNowMs) + m_leadFrames;
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
    // 世界刚被快照复位：此前发出的帧不再代表当前领先量。
    m_leadEffectiveFrame = m_session.currentFrame() + 1;
    resetShrinkWindow();
}

uint64_t NetBattleSync::aheadFrames() const
{
    const uint64_t current   = m_session.currentFrame();
    const uint64_t confirmed = m_session.confirmedFrame();
    return current > confirmed ? current - confirmed : 0;
}

}  // namespace mugen::sim::net
