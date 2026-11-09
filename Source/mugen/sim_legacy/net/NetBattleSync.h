#pragma once

#include "mugen/sim_legacy/net/LockstepAuthority.h"  // kHashInterval / kMaxInputLead
#include "mugen/sim_legacy/rollback/RollbackSession.h"
#include "mugen/sim_legacy/input/FrameInput.h"
#include "mugen/sim_legacy/World.h"

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace mugen::sim_legacy::net
{

// 本地输入延迟的控制模式（调试面板可切换）。
enum class LocalDelayMode : uint8_t
{
    // 按平滑 RTT 自适应：低于阈值为 0，高于阈值覆盖超出部分的一半。
    Auto,
    // 固定为 m_localDelayForcedFrames（逐帧趋近，仍不丢输入）。
    Forced,
    // 关闭，恒为 0。
    Off,
};

// 客户端联网同步的调参项。由 BattleStartPush 的帧参数与既定常量共同确定。
struct NetBattleSyncConfig
{
    // 输入领先量（预测目标帧 = 估计服务器帧 + 领先量）的初始值，来自 BattleStartPush.input_delay_frames；
    // 之后由服务器的到达反馈自适应调整。
    uint32_t m_inputDelayFrames = 2;
    // 自适应领先量的下限与上限（帧）。上限还会被回滚窗口与服务器接受上限封顶。
    uint32_t m_minLeadFrames = 2;
    uint32_t m_maxLeadFrames = 30;
    // 期望本地输入到达服务器时至少还剩的富余帧数（对应抖动余量）；最小富余低于它立即增大领先量。
    int32_t m_slackTargetFrames = 2;
    // 一个观察窗口内最小富余超过「目标 + 该值」才允许缩小领先量，形成滞回避免来回抖动。
    int32_t m_shrinkMarginFrames = 2;
    // 缩小领先量的观察窗口（以服务器反馈的输入帧号跨度计，帧）；每个窗口至少缩小 1 帧，富余越大缩得越多。
    uint32_t m_shrinkWindowFrames = 30;
    // 单次反馈最多增加的领先帧数，避免一次客户端卡顿把领先量顶到上限。
    uint32_t m_maxLeadStep = 8;
    // 开局校准的最长等待（本地毫秒）：首个 RTT 样本到达前本地输入保持不采样（按键状态保留），
    // 超过该时长仍无样本则放行。
    uint32_t m_calibrationTimeoutMs = 2000;
    // 每逻辑帧毫秒数（来自 BattleStartPush.frame_ms）。
    uint32_t m_frameMs = 33;
    // 确定性哈希对账间隔帧数，与服务端一致。
    uint32_t m_hashInterval = kHashInterval;
    // 回滚窗口深度，决定预测允许领先已确认帧的最大帧数。
    uint32_t m_rollbackDepth = rollback::kDefaultRollbackDepth;
    // 时钟偏移 EMA 平滑分母（约 1/10）。
    int64_t m_clockSmoothingDiv = 10;
    // 冗余重发的最近本地输入帧数。
    uint32_t m_redundantInputFrames = 8;

    // 本地输入延迟 D（帧）：采样于第 t 帧的本地输入安排到第 t + D 帧生效并提前发给服务器，
    // 本地预测因此晚 D 帧才执行它，作为交换，客户端领先估计服务器帧的量同步减少 D 帧
    // （领先量 + D 恒等于覆盖 RTT 所需的帧数），本地对远端预测得更少，远端被回滚拉回的距离随之变小。
    LocalDelayMode m_localDelayMode = LocalDelayMode::Auto;
    // Forced 模式下的目标延迟帧数（上限 m_localDelayDebugMaxFrames）。
    uint32_t m_localDelayForcedFrames = 0;
    // 平滑 RTT 不超过该毫秒数时不启用本地输入延迟；超出部分的一半（按帧向上取整）由 D 承担。
    int64_t m_localDelayRttThresholdMs = 250;
    // 自动模式下 D 的上限（帧）。
    uint32_t m_localDelayMaxFrames = 5;
    // 调试强制模式下 D 的上限（帧）。
    uint32_t m_localDelayDebugMaxFrames = 10;
    // 增大 D 的最小间隔（本地毫秒，每次 1 帧）：升得快，但不在同一瞬间连升多帧。
    int64_t m_localDelayRaiseIntervalMs = 100;
    // 减小 D 须持续满足条件的时长（本地毫秒，每次 1 帧）：降得慢。
    int64_t m_localDelayLowerHoldMs = 2000;
    // 减小 D 的 RTT 滞回量（毫秒）：RTT 加上该值之后算出的 D 仍低于当前值才计入降低时长，避免阈值附近来回抖动。
    int64_t m_localDelayLowerMarginMs = 30;
};

// 服务器随 BattleFramesPush 下发的、针对本槽位的输入到达反馈（BattleFramesPush 字段的无协议镜像）。
struct ServerInputFeedback
{
    // 回显的本客户端最近一次推送时间戳（本地单调毫秒）；0 表示无新回显。
    uint64_t m_echoClientTimeMs = 0;
    // 自上次反馈以来首次到达输入的最小富余帧数及其输入帧号；m_slackSamples 为 0 表示无样本。
    int32_t m_slackMin = 0;
    uint64_t m_slackFrame = 0;
    uint32_t m_slackSamples = 0;
    // 服务器累计记录的本槽位迟到输入帧数。
    uint32_t m_lateInputs = 0;
};

// 协议无关的客户端锁步同步逻辑：时钟同步、自适应输入领先量、预测帧目标、输入冗余、哈希对账与重同步决策。
// 不依赖引擎与网络；NetBattleMode 把 protobuf 推送翻译成本类的调用，便于无头回环测试。
class NetBattleSync
{
public:
    NetBattleSync(uint32_t localSlot, uint32_t slotCount, NetBattleSyncConfig config = {});

    // 挂载已初始化世界，记录服务器帧 0 对应的墙钟时间戳，并以当前本地时钟建立首个偏移样本。
    void start(World world, uint64_t startServerTimeMs, uint64_t localNowMs);

    rollback::RollbackSession& session() { return m_session; }
    const rollback::RollbackSession& session() const { return m_session; }

    uint32_t localSlot() const { return m_localSlot; }

    // 用 FramesPush 的 server_frame/server_time_ms 更新时钟偏移估计。
    void onServerClock(uint64_t serverFrame, uint64_t serverTimeMs, uint64_t localNowMs);
    // 消化服务器的输入到达反馈：用回显时间戳测 RTT，用到达富余帧数调整输入领先量。
    // 领先量只影响之后推进的帧，不会跳帧、也不会改写已发送帧的内容：增大时预测多追几帧，
    // 缩小时在 targetFrame 追上之前暂停推进。
    void onServerFeedback(const ServerInputFeedback& feedback, uint64_t localNowMs);

    // 本地输入此刻是否应当采样（frame 为即将推进的帧号）。调用方在返回 false 时跳过输入采样并提交空输入，
    // 按键状态保留（不丢失、不重复），放行后的第一帧照常发出。以下情况返回 false：
    // 1) 首个 RTT 样本到达前：初始领先量远小于 RTT；
    // 2) 领先量刚增大或刚从卡顿 / 重同步追帧期间：追帧生成的帧领先估计服务器帧不足（早已过去），
    //    其输入必然迟到，被服务器顺延执行后客户端要回滚，表现为角色被拉回；
    // 3) 本地输入延迟 D 刚减小：本帧的生效帧（frame + D）已被上一次采样占用，不能改写，留到下一帧采样。
    // 校准超时后无条件放行 1)。
    bool inputReady(uint64_t localNowMs, uint64_t frame) const;
    // 本地时钟下应预测到的目标帧号（估计服务器帧 + 当前输入领先量，受回滚窗口封顶）。
    uint64_t targetFrame(uint64_t localNowMs) const;
    // 由时钟偏移估计服务端当前帧号。
    uint64_t estimatedServerFrame(uint64_t localNowMs) const;
    // 由时钟偏移估计服务端当前时间（本地单调时钟 + 平滑偏移）。
    uint64_t estimatedServerNowMs(uint64_t localNowMs) const;

    // 提交即将推进的 frame 的本地输入（input 为 inputReady 放行时的采样结果，否则为空输入），并记录到冗余环。
    // 采样结果被安排到 frame + D 帧（D 为当前本地输入延迟）：每个帧号恰好获得一份输入且之后不再改写——
    // D 增大时中间空出的帧补空输入，D 减小时调用方因 inputReady 为 false 而不采样，本帧不安排新输入。
    // frame 自身的输入取自此前安排的结果（D = 0 时即本次提交）。
    void submitLocalInput(uint64_t frame, PlayerInput input);
    // 服务器确认单帧输入 / 系统命令（委托给回滚会话）。
    void confirmInput(uint64_t frame, uint32_t slot, PlayerInput input);
    void confirmSystem(uint64_t frame, SystemCommand command);
    // 应用全部已提交确认后立即执行待回滚，刷新世界到一致状态（哈希对账前调用）。
    void flushRollback() { m_session.flushPendingRollback(); }
    // 服务器哈希对账；与本地不一致时返回 true（需重同步）。
    bool checkServerHash(uint64_t frame, uint64_t hash);
    // 收集已确认哈希帧的客户端哈希（供 BattleInputPush.client_hashes，增量）。
    void collectClientHashes(std::vector<std::pair<uint64_t, uint64_t>>& out);
    // 收集仍未确认的本地输入帧（供 BattleInputPush.inputs 冗余重发）。
    void collectPendingInputs(std::vector<std::pair<uint64_t, PlayerInput>>& out);
    // 采用服务器快照复位到指定帧。
    bool resync(uint64_t frame, ByteBuffer& snapshot);
    // 以快照携带的服务器时间直接重置时钟偏移（不做 EMA 平滑）。重连时世界从帧 0 的开始参数
    // 引导、再由快照跳到远大于 0 的当前帧，必须立刻把估计服务器帧对齐到快照帧附近。
    void resetClock(uint64_t serverTimeMs, uint64_t localNowMs);

    // 平滑后的时钟偏移（ms，= 服务端时间 - 本地单调时钟）。
    int64_t clockOffsetMs() const { return m_clockOffsetMs; }
    // 当前自适应输入领先量（帧）：预测目标帧领先估计服务器帧的帧数。
    uint32_t inputLeadFrames() const { return m_leadFrames; }
    // 当前本地输入延迟 D（帧）：本地输入晚 D 帧才在本地预测里生效。
    uint32_t localInputDelayFrames() const { return m_localDelay; }
    // 调试：切换本地输入延迟的控制模式（Auto / Forced / Off）；Forced 时 forcedFrames 为目标 D。
    // D 仍按每次 1 帧趋近目标，切换过程中不丢输入。
    void setLocalDelayMode(LocalDelayMode mode, uint32_t forcedFrames);
    LocalDelayMode localDelayMode() const { return m_config.m_localDelayMode; }
    uint32_t localDelayForcedFrames() const { return m_config.m_localDelayForcedFrames; }
    // 最近一个已安排输入的帧号（含尚未推进到的未来帧）。
    uint64_t scheduledThroughFrame() const { return m_scheduledThrough; }
    // 平滑 RTT 与抖动（ms，含至多一个服务器 tick 的持有时间）；尚无回显样本时为 0。
    int64_t rttMs() const { return m_rttMs; }
    int64_t rttJitterMs() const { return m_rttVarMs; }
    // 服务器累计记录的本槽位迟到输入帧数，以及最近一次反馈的最小富余帧数。
    uint32_t lateInputs() const { return m_lateInputs; }
    int32_t lastSlackFrames() const { return m_lastSlack; }
    // 当前预测领先已确认帧的帧数。
    uint64_t aheadFrames() const;

private:
    // 领先量上限：配置上限，同时留出回滚窗口与服务器接受窗口的余量。
    uint32_t maxLead() const;
    // 缩小领先量的下限：配置下限与按 RTT + 抖动估计的所需帧数（扣除本地输入延迟 D 已承担的部分）取大。
    uint32_t shrinkFloor() const;
    // 设置新领先量；之后真正按新领先量发出（追帧完成后）的帧才算新领先量下的样本。
    void setLead(uint32_t lead, uint64_t localNowMs);
    // 领先量或 D 变化后重置反馈样本的有效起点并清空缩小观察窗口。
    void restartSlackSampling(uint64_t localNowMs);
    // 清空缩小观察窗口。
    void resetShrinkWindow();
    // 自动模式的目标 D：按平滑 RTT 计算（可附加滞回量），受 m_localDelayMaxFrames 封顶。
    uint32_t desiredLocalDelay(int64_t rttMs) const;
    // 每次反馈后按滞回规则把 D 向目标靠拢（每次 1 帧），并让「领先量 + D」保持不变。
    void updateLocalDelay(uint64_t localNowMs);
    // 把 D 改成 delay（相差 1 帧），同步调整领先量并重置反馈样本的有效起点。
    void setLocalDelay(uint32_t delay, uint64_t localNowMs);
    // 安排帧 frame 的本地输入：记入回滚会话与冗余环（恰好一次，不改写）。
    void scheduleLocalInput(uint64_t frame, PlayerInput input);

    // 以「服务端时间 - 本地单调时钟」为样本做 EMA 平滑。
    void updateClockOffset(uint64_t serverTimeMs, uint64_t localNowMs);

    NetBattleSyncConfig m_config;
    uint32_t m_localSlot = 0;
    // 服务器帧 0 对应的墙钟时间戳。
    uint64_t m_startServerTimeMs = 0;
    bool m_hasStart = false;
    // start() 时的本地单调毫秒，用于校准超时。
    uint64_t m_startLocalMs = 0;

    rollback::RollbackSession m_session;

    // 服务端时钟偏移（ms），EMA 平滑。
    int64_t m_clockOffsetMs = 0;
    bool m_hasOffset = false;
    // 当前自适应输入领先量（帧）。
    uint32_t m_leadFrames = 2;
    // 本领先量生效后首个发出的输入帧号（输入帧 = 推进帧 + D）；早于它的反馈样本属于旧领先量 / 旧 D，忽略。
    uint64_t m_leadEffectiveFrame = 0;
    // 缩小观察窗口：起点输入帧号与窗口内最小富余。
    uint64_t m_shrinkWindowStart = 0;
    int32_t m_shrinkWindowMinSlack = 0;
    // RTT 平滑值与平均偏差（ms）。
    int64_t m_rttMs = 0;
    int64_t m_rttVarMs = 0;
    bool m_hasRtt = false;
    uint32_t m_lateInputs = 0;
    int32_t m_lastSlack = 0;
    // 最近一次已发送客户端哈希的帧号。
    uint64_t m_lastSentHashFrame = 0;
    // 最近本地输入帧环（最新在尾部，含已安排但尚未推进到的未来帧），用于冗余重发。
    std::deque<std::pair<uint64_t, PlayerInput>> m_pendingInputs;
    // 当前本地输入延迟 D（帧）。
    uint32_t m_localDelay = 0;
    // 已安排输入的最大帧号：此前每个帧号都恰好有一份已发出的输入。
    uint64_t m_scheduledThrough = 0;
    // 上一次增大 / 调整 D 的本地时间（毫秒），用于增大间隔。
    uint64_t m_localDelayChangedMs = 0;
    // 自动模式下「目标 D 低于当前 D」持续的起点（本地毫秒）；0 表示当前不满足。
    uint64_t m_localDelayLowerSinceMs = 0;
};

}  // namespace mugen::sim_legacy::net
