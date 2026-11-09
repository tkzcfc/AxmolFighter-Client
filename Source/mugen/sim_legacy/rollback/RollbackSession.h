#pragma once
#include "mugen/core/serialize/ByteBuffer.h"
#include "mugen/sim_legacy/World.h"
#include "mugen/sim_legacy/event/PresentationEvent.h"
#include "mugen/sim_legacy/input/FrameInput.h"
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace mugen::sim_legacy::rollback
{
// 默认回滚窗口深度：预测允许领先已确认帧的最多帧数。
inline constexpr uint32_t kDefaultRollbackDepth = 120;

// 回滚会话累计统计，供联机调试与基准输出。
struct RollbackStats
{
    // 实际执行回滚的次数。
    uint64_t rollbacks = 0;
    // 单次回滚重推的最大帧数。
    uint64_t maxDepth = 0;
    // 累计重推的帧数。
    uint64_t resimFrames = 0;
    // 因预测超前超过窗口深度被阻塞的次数。
    uint64_t stalls = 0;
    // 累计发现的错误预测（确认输入与已用输入不一致）次数。
    uint64_t mispredictions = 0;
    // 其中本地槽位的错误预测次数：本地输入被服务器改写（迟到被补空等），不应在正常网络下出现。
    uint64_t localMispredictions = 0;
    // 其中远端槽位的错误预测次数：远端边缘输入（如攻击）与“空输入”预测不符，属预期行为。
    uint64_t remoteMispredictions = 0;
};

// 单槽位单帧的输入条目：实际使用的命令及是否已由服务器确认。
struct SlotFrameEntry
{
    // 该帧实际推入世界的玩家输入。
    PlayerInput input;
    // 该输入是否来自服务器确认。
    bool confirmed = false;
};

// 整个世界的输入预测与回滚会话，编译进客户端与战斗服。
//
// 服务器权威：服务器按帧广播各槽位的确认输入。会话对缺失输入做本地预测
// （本地槽位用 addLocalInput 提交的输入，缺失或远端槽位使用空输入），
// 当确认输入与已用预测不一致时，回滚到最早不一致帧之前的快照并按新输入重推。
// 缺失输入使用空输入而非重复最近确认输入，避免重复触发边缘事件（如攻击按键）。
// 预测帧数受 m_depth 限制，超前过多时停止推进，避免快照被覆盖而无法回滚。
class RollbackSession
{
public:
    // 以本地槽位、槽位数与回滚窗口深度构造空会话；推进前须通过 setWorld 挂载世界。
    RollbackSession(uint32_t localSlot, uint32_t slotCount, uint32_t depth = kDefaultRollbackDepth);
    // 挂载已初始化世界并接管其所有权，重置全部输入与快照状态。
    void setWorld(World world);
    // 挂载后允许调整本地槽位。
    void setLocalSlot(uint32_t slot) { m_localSlot = slot; }

    World& world() { return m_world; }
    const World& world() const { return m_world; }
    uint32_t localSlot() const { return m_localSlot; }
    uint32_t slotCount() const { return m_slotCount; }

    // 本地槽位提交一帧预测输入；若该帧已确认则忽略本地预测。
    void addLocalInput(uint64_t frame, PlayerInput input);
    // 服务器确认一帧某槽位的输入；重复输入幂等，冲突记录错误，允许乱序到达。
    void addConfirmedInput(uint64_t frame, uint32_t slot, PlayerInput input);
    // 服务器确认一帧的房间级系统命令；命令本身不做预测，迟到的命令触发回滚。
    void addConfirmedSystem(uint64_t frame, SystemCommand command);

    // 推进世界到指定帧；预测缺失时按规则补齐。预测超前超过窗口深度时返回 false 且不推进。
    bool advanceTo(uint64_t frame);
    // 推进一帧；预测超前超过窗口深度时返回 false 且不推进。
    bool tick();
    // 下一帧是否仍在预测窗口内；窗口耗尽时记一次停顿并返回 false。调用方在生成本帧输入前先问它，
    // 保证被停顿的帧不会消费（丢弃）一次性的输入命令。
    bool canAdvance();
    // 立即执行待回滚（若有），不推进新帧；供哈希对账前把世界刷新到一致状态。
    void flushPendingRollback();

    // 已模拟到的最新帧号。
    uint64_t currentFrame() const { return m_currentFrame; }
    // 全部槽位均已确认到的最晚帧号。
    uint64_t confirmedFrame() const;
    // 当前世界确定性哈希。
    uint64_t hash() const { return m_world.hash(); }
    // 返回指定帧之后的世界快照哈希；快照已被覆盖时返回 0。
    uint64_t hashAt(uint64_t frame) const;
    // 校验当前世界哈希是否与期望值不一致；不一致返回 true。
    bool checkHash(uint64_t frame, uint64_t expected) const;
    // 采用服务器快照复位到指定帧，丢弃旧输入与快照历史，继续匹配后续确认输入。
    bool resync(uint64_t frame, ByteBuffer& snapshot);

    // 自上次 clearEventsToPresent 以来新产生且去重后的表现事件。
    const std::vector<PresentationEvent>& eventsToPresent() const { return m_eventsToPresent; }
    // 表现层消费完待表现事件后清空。
    void clearEventsToPresent() { m_eventsToPresent.clear(); }

    // 回滚统计。
    const RollbackStats& stats() const { return m_stats; }

private:
    // 判断两个玩家输入是否逐字段一致。
    static bool samePlayerInput(const PlayerInput& a, const PlayerInput& b);
    // 构造下一帧的确定性输入，并记录各槽位实际使用的输入。
    FrameInput buildInput(uint64_t frame);
    // 记录一次错误预测并提前最早回滚帧。
    void markMisprediction(uint64_t frame);
    // 回滚到指定帧之前并重推到回滚前的最新帧。
    bool rollbackTo(uint64_t frame);
    // 内部推进一帧：存快照、构造输入、tick、收集事件。
    void advanceOneFrame();
    // 保存“指定帧之后”的世界快照（用于回滚到该帧的下一帧）。
    void storeSnapshot(uint64_t afterFrame);
    // 判断“指定帧之后”的快照是否仍被保留。
    bool hasSnapshot(uint64_t afterFrame) const;
    // 快照所在的环形槽位。
    uint32_t snapshotIndex(uint64_t afterFrame) const
    {
        return static_cast<uint32_t>(afterFrame % m_depth);
    }
    // 收集指定帧新产生的事件并按稳定标识去重。
    void collectEvents(uint64_t frame);
    // 事件稳定标识：帧、实体、帧内序号与载荷内容的哈希，独立于全局单调 seq。
    static uint64_t eventKey(const PresentationEvent& event);
    // 更新槽位连续确认进度。
    void noteConfirmed(uint32_t slot, uint64_t frame);
    // 重置输入与快照历史到指定帧。
    void resetHistory(uint64_t frame);
    // 丢弃已落出回滚窗口（不可能再被回滚到）的输入与系统命令，避免历史无限增长。
    void pruneHistory();

    uint32_t m_localSlot = 0;
    uint32_t m_slotCount = 1;
    uint32_t m_depth = kDefaultRollbackDepth;

    World m_world;
    // 已模拟到的最新帧号。
    uint64_t m_currentFrame = 0;
    // 环形快照：槽位 i 保存“第 i 帧之后”的世界状态。
    std::vector<World> m_snapshots;
    // 环形快照仍覆盖的最早帧号（其状态可被回滚）。
    uint64_t m_minSnapshotFrame = 0;

    // 每槽位的帧输入条目：实际使用（含预测）与已确认输入。
    std::vector<std::map<uint64_t, SlotFrameEntry>> m_inputs;
    // 本地槽位的预测输入，按帧保存。
    std::map<uint64_t, PlayerInput> m_localInputs;
    // 每槽位连续确认到的最晚帧号。
    std::vector<uint64_t> m_confirmedUpTo;
    // 每帧已确认的房间级系统命令。
    std::map<uint64_t, std::vector<SystemCommand>> m_systemCommands;

    // 待回滚到的最早错误预测帧；0 表示无。
    uint64_t m_pendingRollback = 0;

    // 已表现事件键的每帧环形集合，键为 eventKey 结果。
    std::vector<std::set<uint64_t>> m_presentedKeys;
    std::vector<uint64_t> m_presentedKeyFrame;
    // 待表现层消费的事件。
    std::vector<PresentationEvent> m_eventsToPresent;

    RollbackStats m_stats;
};
}  // namespace mugen::sim_legacy::rollback
