#pragma once
#include "mugen/sim/World.h"
#include "mugen/sim/WorldSetup.h"
#include "mugen/sim/event/PresentationEvent.h"
#include "mugen/sim/input/FrameInput.h"
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace mugen::sim::net
{
// 服务器接受输入允许领先已确认帧的最大帧数。
inline constexpr uint32_t kMaxInputLead = 90;
// 权威世界记录确定性哈希的间隔帧数。
inline constexpr uint32_t kHashInterval = 30;
// 迟到输入（首次到达时目标帧已 tick）最多迟到多少帧仍顺延到下一帧执行；更久的直接丢弃，
// 避免网络长时间卡顿后在很久之后才触发过期的攻击等边缘命令。
inline constexpr uint32_t kMaxLateCarryFrames = 12;
// 每槽位记住已收到输入帧号的回溯窗口（帧）。窗口之外的过期帧既不计迟到也不顺延。
inline constexpr uint32_t kReceivedWindowFrames = 64;

// 单槽位输入到达质量的统计，仅作反馈用途，不参与世界哈希。
struct SlotInputStats
{
    // 累计首次到达时目标帧已 tick 的输入帧数（含空输入）。
    uint32_t m_lateInputs = 0;
    // 其中非空且顺延到下一帧执行的帧数。
    uint32_t m_carriedInputs = 0;
    // 其中非空但迟到过久被丢弃的帧数。
    uint32_t m_droppedInputs = 0;
    // 自上次 resetSlackWindow 以来首次到达输入的最小富余帧数（输入帧号 - 已 tick 帧号 - 1，负数为迟到）、
    // 对应输入帧号与样本数。
    int32_t m_windowMinSlack = 0;
    uint64_t m_windowMinFrame = 0;
    uint32_t m_windowSamples = 0;
};

// PVP 决斗结束后的权威结果，独立于任何本地阵营视角。
struct BattleOutcome
{
    // 获胜阵营位；零表示平局。
    int32_t m_winnerCamp = 0;
    // 结束原因：KO 或超时。
    PvpRoundEndReason m_reason = PvpRoundEndReason::Ko;
};

// 服务器权威的锁步世界：持有唯一世界实例，按帧接受各槽位输入，
// 广播每帧确认输入，并记录确定性哈希供客户端校验。纯逻辑、确定性、无网络依赖。
class LockstepAuthority
{
public:
    // 以世界配置构造权威世界；bootstrapBattle 负责出生全部玩家并进入房间。
    explicit LockstepAuthority(const WorldSetup& setup);

    // 该槽位的指定帧输入是否还需要解码提交：未超前过多，且此前没有收到过该帧。
    // 客户端每次推送都会冗余重发最近若干帧，调用方据此在解码前跳过重复帧。
    bool needsInput(uint32_t slot, uint64_t frame) const;
    // 提交某槽位在指定帧的输入。重复提交同一帧保持首次提交；超前过多返回 false。
    // 首次到达时目标帧已 tick 的输入记为迟到：非空命令且迟到不超过 kMaxLateCarryFrames 时
    // 顺延到下一帧、排在该帧自身命令之前（保持命令先后顺序）并返回 true，否则丢弃并返回 false。
    bool submitInput(uint32_t slot, uint64_t frame, PlayerInput input);
    // 提交指定帧的房间级系统命令；与输入采用相同的领先限制。
    bool submitSystem(uint64_t frame, SystemCommand command);

    // 槽位失联时调用：丢弃该槽位已排队的全部输入，并在下一帧排入一条待机命令
    // （等同于松开所有按键），其后缺失输入按空处理，角色停在原地而不是保持断线前的跑动。
    void releaseSlot(uint32_t slot);

    // 指定槽位的输入到达统计。
    const SlotInputStats& inputStats(uint32_t slot) const { return m_stats[slot]; }
    // 清空该槽位的富余帧数统计窗口（累计计数不变）。
    void resetSlackWindow(uint32_t slot);

    // 构建并执行下一帧的确认输入，返回该帧确认输入。
    const FrameInput& step();

    // 已确认到的最新帧号。
    uint64_t currentFrame() const { return m_lastTickedFrame; }
    // 本场玩家槽位数。
    uint32_t slotCount() const { return m_slotCount; }
    // 返回指定帧之后的世界快照哈希；未记录时返回 0。
    uint64_t hashAt(uint64_t frame) const;
    // 已按间隔记录的帧号到哈希映射。
    const std::map<uint64_t, uint64_t>& hashes() const { return m_hashes; }
    // PVP 决斗结束后的结果；未结束时返回空。
    std::optional<BattleOutcome> outcome() const { return m_outcome; }
    // 序列化当前世界快照到字节串，供客户端 resync。
    void serializeWorld(std::string& out) const;
    // 只读访问权威世界。
    const World& world() const { return m_world; }

private:
    // 应用下一帧输入并推进世界，同时收集结果与哈希。
    void advanceOneFrame(uint64_t frame);

    // 权威世界实例。
    World m_world;
    // 玩家槽位数。
    uint32_t m_slotCount = 0;
    // 已确认到的最新帧号。
    uint64_t m_lastTickedFrame = 0;
    // 各槽位待消费的输入，按帧保存。
    std::vector<std::map<uint64_t, PlayerInput>> m_submitted;
    // 各槽位已收到过的输入帧号（回溯 kReceivedWindowFrames 帧），用于区分重复重发与首次迟到。
    std::vector<std::set<uint64_t>> m_received;
    // 各槽位顺延到下一帧的迟到命令，按原输入帧号排序。
    std::vector<std::map<uint64_t, std::vector<PlayerCommand>>> m_carry;
    // 各槽位输入到达统计。
    std::vector<SlotInputStats> m_stats;
    // 按帧排队的房间级系统命令。
    std::map<uint64_t, std::vector<SystemCommand>> m_systemCommands;
    // 最近一次 step() 构建的确认输入。
    FrameInput m_lastInput;
    // 按间隔帧号记录的确定性哈希。
    std::map<uint64_t, uint64_t> m_hashes;
    // PVP 决斗结果，一旦产生即固定。
    std::optional<BattleOutcome> m_outcome;
};
}  // namespace mugen::sim::net
