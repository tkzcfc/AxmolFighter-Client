#include "LockstepAuthority.h"

#include "mugen/sim_legacy/BattleBootstrap.h"
#include "mugen/core/serialize/BinaryArchive.h"

#include <utility>

namespace mugen::sim_legacy::net
{
LockstepAuthority::LockstepAuthority(const WorldSetup& setup)
{
    m_slotCount = static_cast<uint32_t>(setup.m_players.size());
    const auto heroes = bootstrapBattle(m_world, setup);
    if (heroes.empty())
    {
        MG_LOG_E("LockstepAuthority failed to bootstrap battle room {}", setup.roomId);
        MG_ASSERT(!heroes.empty() && "LockstepAuthority bootstrap failed");
        return;
    }
    m_submitted.resize(m_slotCount);
    m_received.resize(m_slotCount);
    m_carry.resize(m_slotCount);
    m_stats.resize(m_slotCount);
    m_lastTickedFrame = m_world.ecs().singleton<WorldClock>().frame;
}

bool LockstepAuthority::needsInput(uint32_t slot, uint64_t frame) const
{
    if (slot >= m_slotCount || frame == 0)
        return false;
    if (frame > m_lastTickedFrame + kMaxInputLead)
        return false;
    // 已落出回溯窗口的过期帧无法区分「收到过」与「从未收到」，一律忽略。
    if (frame + kReceivedWindowFrames <= m_lastTickedFrame)
        return false;
    return !m_received[slot].contains(frame);
}

bool LockstepAuthority::submitInput(uint32_t slot, uint64_t frame, PlayerInput input)
{
    if (slot >= m_slotCount)
    {
        MG_LOG_E("submitInput slot out of range: {}", slot);
        return false;
    }
    if (!needsInput(slot, frame))
    {
        // 重复提交保持首次提交并视为成功；零帧、超前过多与已落出回溯窗口的过期帧被拒绝。
        const bool duplicate = frame != 0 && frame <= m_lastTickedFrame + kMaxInputLead &&
                               frame + kReceivedWindowFrames > m_lastTickedFrame;
        if (!duplicate)
            MG_LOG_D("submitInput rejected frame {} for slot {} (ticked {})", frame, slot, m_lastTickedFrame);
        return duplicate;
    }

    // 富余帧数：输入还能领先被消费帧多少帧，0 刚好赶上，负数迟到。
    const int32_t slack = static_cast<int32_t>(static_cast<int64_t>(frame) - static_cast<int64_t>(m_lastTickedFrame) - 1);
    auto& stats = m_stats[slot];
    if (stats.m_windowSamples == 0 || slack < stats.m_windowMinSlack)
    {
        stats.m_windowMinSlack = slack;
        stats.m_windowMinFrame = frame;
    }
    ++stats.m_windowSamples;
    m_received[slot].insert(frame);

    input.slot = slot;
    if (frame > m_lastTickedFrame)
    {
        m_submitted[slot].emplace(frame, std::move(input));
        return true;
    }

    // 迟到：目标帧已被权威 tick（当时按空输入处理）。
    ++stats.m_lateInputs;
    if (input.commands.empty())
        return true;
    if (m_lastTickedFrame - frame >= kMaxLateCarryFrames)
    {
        // 过期太久：攻击等一次性边缘命令丢弃；移动状态命令（跑动 / 待机）是电平语义，只在方向变化时发出一次，
        // 丢掉会让角色永久停留在旧状态（客户端回滚后也无从恢复），因此只保留其中最后一条继续顺延。
        for (auto it = input.commands.rbegin(); it != input.commands.rend(); ++it)
        {
            if (it->controlEvent != ControlEvent::CTR_RUN && it->controlEvent != ControlEvent::CTR_IDLE)
                continue;
            ++stats.m_carriedInputs;
            m_carry[slot].emplace(frame, std::vector<PlayerCommand>{*it});
            MG_LOG_D("submitInput carried stale move state frame {} for slot {} (ticked {})", frame, slot, m_lastTickedFrame);
            return true;
        }
        ++stats.m_droppedInputs;
        MG_LOG_D("submitInput dropped stale input frame {} for slot {} (ticked {})", frame, slot, m_lastTickedFrame);
        return false;
    }
    ++stats.m_carriedInputs;
    m_carry[slot].emplace(frame, std::move(input.commands));
    MG_LOG_D("submitInput carried late input frame {} for slot {} (ticked {})", frame, slot, m_lastTickedFrame);
    return true;
}

void LockstepAuthority::resetSlackWindow(uint32_t slot)
{
    auto& stats = m_stats[slot];
    stats.m_windowMinSlack = 0;
    stats.m_windowMinFrame = 0;
    stats.m_windowSamples  = 0;
}

bool LockstepAuthority::submitSystem(uint64_t frame, SystemCommand command)
{
    if (frame <= m_lastTickedFrame)
    {
        MG_LOG_D("submitSystem late frame {} (ticked {})", frame, m_lastTickedFrame);
        return false;
    }
    if (frame > m_lastTickedFrame + kMaxInputLead)
    {
        MG_LOG_D("submitSystem frame {} too far ahead (ticked {})", frame, m_lastTickedFrame);
        return false;
    }
    m_systemCommands[frame].push_back(std::move(command));
    return true;
}

void LockstepAuthority::releaseSlot(uint32_t slot)
{
    if (slot >= m_slotCount)
    {
        MG_LOG_E("releaseSlot slot out of range: {}", slot);
        return;
    }
    m_submitted[slot].clear();
    m_carry[slot].clear();
    m_received[slot].clear();
    PlayerInput idle;
    idle.slot = slot;
    idle.commands.push_back({ControlEvent::CTR_IDLE, ControlQuadrant::None, 0, 0});
    m_submitted[slot].emplace(m_lastTickedFrame + 1, std::move(idle));
}

const FrameInput& LockstepAuthority::step()
{
    const uint64_t frame = m_lastTickedFrame + 1;
    advanceOneFrame(frame);
    return m_lastInput;
}

void LockstepAuthority::advanceOneFrame(uint64_t frame)
{
    FrameInput input;
    input.frame = frame;
    for (uint32_t slot = 0; slot < m_slotCount; ++slot)
    {
        PlayerInput used;
        used.slot = slot;
        auto& submitted = m_submitted[slot];
        const auto it   = submitted.find(frame);
        if (it != submitted.end())
            used = it->second;
        else
            used.commands.clear();  // 缺失输入使用空输入，与客户端预测规则一致。
        submitted.erase(submitted.begin(), submitted.upper_bound(frame));
        // 顺延的迟到命令排在本帧自身命令之前，保持原先后顺序。
        auto& carry = m_carry[slot];
        if (!carry.empty())
        {
            std::vector<PlayerCommand> merged;
            for (auto& [lateFrame, commands] : carry)
                merged.insert(merged.end(), commands.begin(), commands.end());
            merged.insert(merged.end(), used.commands.begin(), used.commands.end());
            used.commands = std::move(merged);
            carry.clear();
        }
        // 回溯窗口外的已收帧号不再需要。
        auto& received = m_received[slot];
        received.erase(received.begin(), received.lower_bound(frame > kReceivedWindowFrames ? frame - kReceivedWindowFrames + 1 : 0));
        input.players.push_back(std::move(used));
    }
    const auto sysIt = m_systemCommands.find(frame);
    if (sysIt != m_systemCommands.end())
    {
        input.systemCommands = std::move(sysIt->second);
        m_systemCommands.erase(sysIt);
    }

    m_world.tick(input);
    m_lastTickedFrame = frame;
    m_lastInput       = std::move(input);

    if (frame % kHashInterval == 0)
        m_hashes[frame] = m_world.hash();

    if (!m_outcome)
        for (const auto& event : m_world.ecs().singleton<PresentationEvents>().events)
            if (const auto* end = std::get_if<PvpRoundEnd>(&event.payload))
                m_outcome = BattleOutcome{end->m_winnerCamp, end->m_reason};
}

uint64_t LockstepAuthority::hashAt(uint64_t frame) const
{
    if (frame == m_lastTickedFrame)
        return m_world.hash();
    const auto it = m_hashes.find(frame);
    return it == m_hashes.end() ? 0 : it->second;
}

void LockstepAuthority::serializeWorld(std::string& out) const
{
    ByteBuffer buffer;
    m_world.serialize(buffer);
    out.assign(reinterpret_cast<const char*>(buffer.data()), buffer.len());
}
}  // namespace mugen::sim_legacy::net
