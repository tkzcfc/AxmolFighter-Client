#include "NetBattleSync.h"

#include <algorithm>
#include <utility>

namespace mugen::sim_legacy::net
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
    m_startLocalMs      = localNowMs;
    m_lastSentHashFrame = 0;
    m_pendingInputs.clear();
    m_localDelay             = 0;
    m_localDelayChangedMs    = 0;
    m_localDelayLowerSinceMs = 0;
    m_scheduledThrough       = m_session.currentFrame();
    m_leadFrames = std::clamp(m_config.m_inputDelayFrames, m_config.m_minLeadFrames, maxLead());
    m_leadEffectiveFrame = m_scheduledThrough + 1;
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
    // 领先量不得超过回滚窗口（留出重推余量）与服务器接受输入的上限（留出时钟估计误差）；
    // 输入帧 = 推进帧 + D，所以服务器上限还要扣掉 D。
    const uint32_t byDepth = m_config.m_rollbackDepth > 16 ? m_config.m_rollbackDepth - 16 : 1;
    uint32_t byServer      = kMaxInputLead > 8 ? kMaxInputLead - 8 : 1;
    byServer               = byServer > m_localDelay + 1 ? byServer - m_localDelay : 1;
    return std::max(m_config.m_minLeadFrames, std::min({m_config.m_maxLeadFrames, byDepth, byServer}));
}

uint32_t NetBattleSync::shrinkFloor() const
{
    uint32_t floor = m_config.m_minLeadFrames;
    if (m_hasRtt)
    {
        // 估计时钟落后真实服务器约一个单向时延，输入再花一个单向时延才到：至少需要一个 RTT（含抖动）的领先；
        // 其中 D 帧由本地输入延迟承担（输入比推进提前 D 帧发出），领先量只需覆盖其余部分。
        const int64_t rttHigh = m_rttMs + 2 * m_rttVarMs;
        const auto frameMs    = static_cast<int64_t>(m_config.m_frameMs);
        const auto needed     = static_cast<uint32_t>((rttHigh + frameMs - 1) / frameMs);
        floor = std::max(floor, needed > m_localDelay ? needed - m_localDelay : 0u);
    }
    return std::min(floor, maxLead());
}

void NetBattleSync::setLead(uint32_t lead, uint64_t localNowMs)
{
    lead = std::clamp(lead, m_config.m_minLeadFrames, maxLead());
    if (lead == m_leadFrames)
        return;
    m_leadFrames = lead;
    restartSlackSampling(localNowMs);
}

void NetBattleSync::restartSlackSampling(uint64_t localNowMs)
{
    // 领先量（或 D）变化后要先追帧 / 等待：追帧期间生成的帧（帧号低于 估计服务器帧 + 新领先量 - 容差）按 inputReady
    // 不采样输入、本来就落后于服务器，它们的富余不代表新设置，必须和旧设置下发出的帧一并忽略，否则会连续超调。
    // 比较对象是输入帧号（= 推进帧 + D），所以起点要加上 D。
    const auto tolerance = static_cast<uint64_t>(std::max(m_config.m_slackTargetFrames, 0));
    const uint64_t first = estimatedServerFrame(localNowMs) + m_leadFrames + m_localDelay;
    m_leadEffectiveFrame = std::max(m_scheduledThrough + 1, first > tolerance ? first - tolerance : 0);
    resetShrinkWindow();
}

void NetBattleSync::resetShrinkWindow()
{
    m_shrinkWindowStart    = 0;
    m_shrinkWindowMinSlack = 0;
}

uint32_t NetBattleSync::desiredLocalDelay(int64_t rttMs) const
{
    if (rttMs <= m_config.m_localDelayRttThresholdMs)
        return 0;
    // 覆盖超出阈值部分的一半：D = ceil((rtt - 阈值) / 2 / 帧时长)。
    const int64_t twoFrames = 2 * static_cast<int64_t>(m_config.m_frameMs);
    const int64_t excess    = rttMs - m_config.m_localDelayRttThresholdMs;
    const auto frames       = static_cast<uint32_t>((excess + twoFrames - 1) / twoFrames);
    return std::min(frames, m_config.m_localDelayMaxFrames);
}

void NetBattleSync::setLocalDelayMode(LocalDelayMode mode, uint32_t forcedFrames)
{
    m_config.m_localDelayMode         = mode;
    m_config.m_localDelayForcedFrames = std::min(forcedFrames, m_config.m_localDelayDebugMaxFrames);
    m_localDelayLowerSinceMs          = 0;
}

void NetBattleSync::updateLocalDelay(uint64_t localNowMs)
{
    const bool automatic = m_config.m_localDelayMode == LocalDelayMode::Auto;
    uint32_t target      = 0;
    if (m_config.m_localDelayMode == LocalDelayMode::Forced)
        target = m_config.m_localDelayForcedFrames;
    else if (automatic && m_hasRtt)
        target = desiredLocalDelay(m_rttMs);

    const auto sinceChange = static_cast<int64_t>(localNowMs - m_localDelayChangedMs);
    if (target > m_localDelay)
    {
        // 升得快：每个间隔 +1 帧。
        m_localDelayLowerSinceMs = 0;
        if (sinceChange >= m_config.m_localDelayRaiseIntervalMs)
            setLocalDelay(m_localDelay + 1, localNowMs);
        return;
    }
    if (target == m_localDelay)
    {
        m_localDelayLowerSinceMs = 0;
        return;
    }
    if (!automatic)
    {
        // 调试强制 / 关闭：不需要滞回，按升的节奏逐帧降。
        if (sinceChange >= m_config.m_localDelayRaiseIntervalMs)
            setLocalDelay(m_localDelay - 1, localNowMs);
        return;
    }
    // 降得慢：RTT 加上滞回量后算出的目标仍低于当前 D，且持续一段时间，才每次降 1 帧。
    if (desiredLocalDelay(m_rttMs + m_config.m_localDelayLowerMarginMs) >= m_localDelay)
    {
        m_localDelayLowerSinceMs = 0;
        return;
    }
    if (m_localDelayLowerSinceMs == 0)
        m_localDelayLowerSinceMs = localNowMs;
    if (static_cast<int64_t>(localNowMs - m_localDelayLowerSinceMs) >= m_config.m_localDelayLowerHoldMs)
    {
        setLocalDelay(m_localDelay - 1, localNowMs);
        m_localDelayLowerSinceMs = 0;
    }
}

void NetBattleSync::setLocalDelay(uint32_t delay, uint64_t localNowMs)
{
    const uint32_t old    = m_localDelay;
    m_localDelay          = delay;
    m_localDelayChangedMs = localNowMs;
    // 输入帧 = 推进帧 + D 且服务器看到的输入时刻不变：领先量 + D 保持不变，
    // D 增大则本地少领先 D 帧，D 减小则多领先（一次 1 帧，追帧由调用方的推进循环完成）。
    if (delay > old)
        setLead(m_leadFrames > delay - old ? m_leadFrames - (delay - old) : 0, localNowMs);
    else
        setLead(m_leadFrames + (old - delay), localNowMs);
    // 即使领先量被下限 / 上限钳住没有变化，D 变化后的输入帧富余也与此前不同。
    restartSlackSampling(localNowMs);
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
            // 首个样本直接把领先量抬到覆盖 RTT（外加四分之一的抖动余量），不必等迟到反馈一轮轮补：
            // 开局输入据此一开始就不迟到；之后缩小受 RTT 下限与滞回约束。
            const auto frameMs = static_cast<int64_t>(m_config.m_frameMs);
            const auto rttLead = static_cast<uint32_t>((sample + sample / 4 + frameMs - 1) / frameMs) +
                                 static_cast<uint32_t>(std::max(m_config.m_slackTargetFrames, 0));
            if (rttLead > m_leadFrames)
                setLead(rttLead, localNowMs);
        }
        else
        {
            const int64_t deviation = sample > m_rttMs ? sample - m_rttMs : m_rttMs - sample;
            m_rttVarMs += (deviation - m_rttVarMs) / 4;
            m_rttMs += (sample - m_rttMs) / 8;
        }
    }

    // 本地输入延迟随平滑 RTT 调整；它改变领先量与有效起点，必须在处理本次富余样本之前完成。
    updateLocalDelay(localNowMs);

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
        setLead(m_leadFrames + std::min(deficit, m_config.m_maxLeadStep), localNowMs);
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
        setLead(m_leadFrames - std::min(step, m_leadFrames - floor), localNowMs);
    }
    resetShrinkWindow();
}

bool NetBattleSync::inputReady(uint64_t localNowMs, uint64_t frame) const
{
    // 本帧的生效帧（frame + D）已被上一次采样占用（D 刚减小，或本帧已提交过）：不能改写，也不能消费按键。
    if (frame + m_localDelay <= m_scheduledThrough)
        return false;
    if (!m_hasRtt)
        return localNowMs - m_startLocalMs >= m_config.m_calibrationTimeoutMs;
    // 稳态下每帧恰好领先估计服务器帧「领先量」帧；允许少量时钟估计抖动（一个富余目标的帧数）。
    const auto tolerance = static_cast<uint64_t>(std::max(m_config.m_slackTargetFrames, 0));
    return frame + tolerance >= estimatedServerFrame(localNowMs) + m_leadFrames;
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

void NetBattleSync::scheduleLocalInput(uint64_t frame, PlayerInput input)
{
    input.slot = m_localSlot;
    m_session.addLocalInput(frame, input);
    m_pendingInputs.emplace_back(frame, std::move(input));
    m_scheduledThrough = frame;
    // 环内含 D 个尚未推进到的未来帧，容量随 D 加大，保证已推进帧的冗余重发数量不变。
    while (m_pendingInputs.size() > m_config.m_redundantInputFrames + m_localDelay)
        m_pendingInputs.pop_front();
}

void NetBattleSync::submitLocalInput(uint64_t frame, PlayerInput input)
{
    const uint64_t slot = frame + m_localDelay;
    // 该生效帧已有输入（重复提交 / D 减小）：已发出的内容不再改写。
    if (slot <= m_scheduledThrough)
        return;
    // D 增大后中间空出的帧补空输入，保证每个帧号恰好一份输入。
    for (uint64_t gap = m_scheduledThrough + 1; gap < slot; ++gap)
        scheduleLocalInput(gap, PlayerInput{});
    scheduleLocalInput(slot, std::move(input));
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
    if (!m_session.resync(frame, snapshot))
        return false;
    // 会话复位清空了本地输入：把已发给服务器、仍在快照帧之后的输入原样放回（内容与服务器一致，不产生错误预测），
    // 并保证之后不会为已发出的帧重新安排不同的输入。
    for (const auto& entry : m_pendingInputs)
        if (entry.first > frame)
            m_session.addLocalInput(entry.first, entry.second);
    m_scheduledThrough = std::max(m_scheduledThrough, frame);
    return true;
}

void NetBattleSync::resetClock(uint64_t serverTimeMs, uint64_t localNowMs)
{
    m_clockOffsetMs = static_cast<int64_t>(serverTimeMs) - static_cast<int64_t>(localNowMs);
    m_hasOffset     = true;
    // 世界刚被快照复位：此前发出的帧不再代表当前领先量。
    m_leadEffectiveFrame = m_scheduledThrough + 1;
    resetShrinkWindow();
}

uint64_t NetBattleSync::aheadFrames() const
{
    const uint64_t current   = m_session.currentFrame();
    const uint64_t confirmed = m_session.confirmedFrame();
    return current > confirmed ? current - confirmed : 0;
}

}  // namespace mugen::sim_legacy::net
