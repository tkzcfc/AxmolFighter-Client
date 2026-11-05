#include "LockstepAuthority.h"

#include "mugen/sim/BattleBootstrap.h"
#include "mugen/core/serialize/BinaryArchive.h"

#include <utility>

namespace mugen::sim::net
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
    m_lastTickedFrame = m_world.ecs().singleton<WorldClock>().frame;
}

bool LockstepAuthority::submitInput(uint32_t slot, uint64_t frame, PlayerInput input)
{
    if (slot >= m_slotCount)
    {
        MG_LOG_E("submitInput slot out of range: {}", slot);
        return false;
    }
    if (frame <= m_lastTickedFrame)
    {
        MG_LOG_D("submitInput late frame {} for slot {} (ticked {})", frame, slot, m_lastTickedFrame);
        return false;
    }
    if (frame > m_lastTickedFrame + kMaxInputLead)
    {
        MG_LOG_D("submitInput frame {} too far ahead for slot {} (ticked {})", frame, slot, m_lastTickedFrame);
        return false;
    }
    input.slot = slot;
    m_submitted[slot].emplace(frame, std::move(input));
    return true;
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
}  // namespace mugen::sim::net
