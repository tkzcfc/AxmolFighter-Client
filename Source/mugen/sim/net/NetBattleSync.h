#pragma once

#include "mugen/sim/net/LockstepAuthority.h"  // kHashInterval / kMaxInputLead
#include "mugen/sim/rollback/RollbackSession.h"
#include "mugen/sim/input/FrameInput.h"
#include "mugen/sim/World.h"

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace mugen::sim::net
{

// 客户端联网同步的调参项。由 BattleStartPush 的帧参数与既定常量共同确定。
struct NetBattleSyncConfig
{
    // 客户端提交输入预留的帧延迟（来自 BattleStartPush.input_delay_frames）。
    uint32_t m_inputDelayFrames = 2;
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
};

// 协议无关的客户端锁步同步逻辑：时钟同步、预测帧目标、输入冗余、哈希对账与重同步决策。
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

    // 用 FramesPush 的 server_frame/server_time_ms 更新时钟偏移与单向延迟估计。
    void onServerClock(uint64_t serverFrame, uint64_t serverTimeMs, uint64_t localNowMs);

    // 本地时钟下应预测到的目标帧号（估计服务器帧 + 输入延迟，受回滚窗口封顶）。
    uint64_t targetFrame(uint64_t localNowMs) const;
    // 由时钟偏移估计服务端当前帧号。
    uint64_t estimatedServerFrame(uint64_t localNowMs) const;
    // 由时钟偏移估计服务端当前时间（本地单调时钟 + 平滑偏移）。
    uint64_t estimatedServerNowMs(uint64_t localNowMs) const;

    // 提交本地预测输入并记录到冗余环。
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
    // 平滑后的单向延迟估计（ms），供调试面板展示。
    int64_t estimatedOneWayDelayMs() const { return m_pingMs; }
    // 当前预测领先已确认帧的帧数。
    uint64_t leadFrames() const;

private:
    // 以「服务端时间 - 本地单调时钟」为样本做 EMA 平滑。
    void updateClockOffset(uint64_t serverTimeMs, uint64_t localNowMs);

    NetBattleSyncConfig m_config;
    uint32_t m_localSlot = 0;
    // 服务器帧 0 对应的墙钟时间戳。
    uint64_t m_startServerTimeMs = 0;
    bool m_hasStart = false;

    rollback::RollbackSession m_session;

    // 服务端时钟偏移（ms），EMA 平滑。
    int64_t m_clockOffsetMs = 0;
    bool m_hasOffset = false;
    // 单向延迟估计（ms），EMA 平滑。
    int64_t m_pingMs = 0;
    bool m_hasPing = false;
    // 最近一次已发送客户端哈希的帧号。
    uint64_t m_lastSentHashFrame = 0;
    // 最近本地输入帧环（最新在尾部），用于冗余重发。
    std::deque<std::pair<uint64_t, PlayerInput>> m_pendingInputs;
};

}  // namespace mugen::sim::net
