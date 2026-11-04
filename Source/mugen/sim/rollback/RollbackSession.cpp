#include "RollbackSession.h"

#include "mugen/core/serialize/Hash.h"

namespace mugen::sim::rollback
{
namespace
{
// 已表现事件键环的“未占用”哨兵帧号。
constexpr uint64_t kNoFrame = std::numeric_limits<uint64_t>::max();
}  // namespace

RollbackSession::RollbackSession(uint32_t localSlot, uint32_t slotCount, uint32_t depth)
    : m_localSlot(localSlot), m_slotCount(slotCount), m_depth(depth)
{
    if (m_depth == 0)
        m_depth = kDefaultRollbackDepth;
    if (m_slotCount == 0)
        m_slotCount = 1;
    m_snapshots.resize(m_depth);
    m_inputs.resize(m_slotCount);
    m_lastConfirmedInput.resize(m_slotCount);
    m_lastConfirmedFrame.assign(m_slotCount, 0);
    m_confirmedUpTo.assign(m_slotCount, 0);
    m_presentedKeys.resize(m_depth);
    m_presentedKeyFrame.assign(m_depth, kNoFrame);
}

void RollbackSession::setWorld(World world)
{
    m_world       = std::move(world);
    m_currentFrame = m_world.ecs().singleton<WorldClock>().frame;
    m_minSnapshotFrame = 0;
    m_snapshots[snapshotIndex(0)] = m_world;
    m_localInputs.clear();
    m_systemCommands.clear();
    for (auto& slot : m_inputs)
        slot.clear();
    for (uint32_t slot = 0; slot < m_slotCount; ++slot)
    {
        m_lastConfirmedInput[slot]  = PlayerInput{slot, {}};
        m_lastConfirmedFrame[slot]  = 0;
        m_confirmedUpTo[slot]       = 0;
    }
    m_pendingRollback = 0;
    m_stats           = {};
    for (auto& keys : m_presentedKeys)
        keys.clear();
    for (auto& frame : m_presentedKeyFrame)
        frame = kNoFrame;
    m_eventsToPresent.clear();
}

void RollbackSession::addLocalInput(uint64_t frame, PlayerInput input)
{
    input.slot = m_localSlot;
    m_localInputs[frame] = std::move(input);
}

void RollbackSession::addConfirmedInput(uint64_t frame, uint32_t slot, PlayerInput input)
{
    if (frame == 0 || slot >= m_slotCount)
    {
        MG_LOG_E("addConfirmedInput out of range: slot {} frame {}", slot, frame);
        MG_ASSERT(frame != 0 && slot < m_slotCount);
        return;
    }
    input.slot = slot;
    auto& entry = m_inputs[slot][frame];
    if (entry.confirmed)
    {
        if (!samePlayerInput(entry.input, input))
            MG_LOG_E("conflicting confirmed inputs for slot {} frame {}", slot, frame);
        return;
    }
    // 该帧已用预测输入模拟过且与确认不一致时，记录错误预测并回滚。
    if (frame <= m_currentFrame && !samePlayerInput(entry.input, input))
        markMisprediction(frame);
    entry = {input, true};
    noteConfirmed(slot, frame, input);
}

void RollbackSession::addConfirmedSystem(uint64_t frame, SystemCommand command)
{
    if (frame == 0)
    {
        MG_LOG_E("addConfirmedSystem frame must be positive");
        MG_ASSERT(frame != 0);
        return;
    }
    auto& commands     = m_systemCommands[frame];
    const uint64_t key = hashCanonical(command);
    for (const auto& existing : commands)
        if (hashCanonical(existing) == key)
            return;
    commands.push_back(std::move(command));
    if (frame <= m_currentFrame)
        markMisprediction(frame);
}

bool RollbackSession::advanceTo(uint64_t frame)
{
    while (m_currentFrame < frame)
    {
        if (!tick())
            return false;
    }
    return true;
}

bool RollbackSession::tick()
{
    // 先执行待回滚，再决定是否推进下一帧。
    if (m_pendingRollback != 0)
    {
        if (!rollbackTo(m_pendingRollback))
            return false;
    }
    const uint64_t nextFrame = m_currentFrame + 1;
    if (nextFrame - confirmedFrame() > m_depth)
    {
        ++m_stats.stalls;
        return false;
    }
    advanceOneFrame();
    return true;
}

uint64_t RollbackSession::confirmedFrame() const
{
    uint64_t result = std::numeric_limits<uint64_t>::max();
    for (uint32_t slot = 0; slot < m_slotCount; ++slot)
        result = std::min(result, m_confirmedUpTo[slot]);
    return result == std::numeric_limits<uint64_t>::max() ? 0 : result;
}

bool RollbackSession::checkHash(uint64_t frame, uint64_t expected) const
{
    if (frame != m_currentFrame)
        MG_LOG_W("checkHash frame {} does not match current frame {}", frame, m_currentFrame);
    return m_world.hash() != expected;
}

uint64_t RollbackSession::hashAt(uint64_t frame) const
{
    if (frame == m_currentFrame)
        return m_world.hash();
    if (!hasSnapshot(frame))
        return 0;
    return m_snapshots[snapshotIndex(frame)].hash();
}

bool RollbackSession::resync(uint64_t frame, ByteBuffer& snapshot)
{
    if (!m_world.deserialize(snapshot))
    {
        MG_LOG_E("resync snapshot deserialize failed");
        return false;
    }
    const uint64_t restoredFrame = m_world.ecs().singleton<WorldClock>().frame;
    if (restoredFrame != frame)
        MG_LOG_W("resync frame {} does not match snapshot frame {}", frame, restoredFrame);
    resetHistory(restoredFrame);
    return true;
}

bool RollbackSession::samePlayerInput(const PlayerInput& a, const PlayerInput& b)
{
    if (a.slot != b.slot || a.commands.size() != b.commands.size())
        return false;
    for (size_t index = 0; index < a.commands.size(); ++index)
    {
        const PlayerCommand& ca = a.commands[index];
        const PlayerCommand& cb = b.commands[index];
        if (ca.controlEvent != cb.controlEvent || ca.quadrant != cb.quadrant || ca.angle != cb.angle ||
            ca.pressType != cb.pressType)
            return false;
    }
    return true;
}

FrameInput RollbackSession::buildInput(uint64_t frame)
{
    FrameInput input;
    input.frame = frame;
    for (uint32_t slot = 0; slot < m_slotCount; ++slot)
    {
        PlayerInput used;
        used.slot = slot;
        const auto it = m_inputs[slot].find(frame);
        if (it != m_inputs[slot].end() && it->second.confirmed)
        {
            used = it->second.input;
        }
        else if (slot == m_localSlot)
        {
            const auto local = m_localInputs.find(frame);
            if (local != m_localInputs.end())
                used = local->second;
            else
                used.commands.clear();
        }
        else
        {
            // 远端槽位预测：重复该槽位最近一次确认输入；从未确认则为空输入。
            used = m_lastConfirmedInput[slot];
        }
        if (it == m_inputs[slot].end() || !it->second.confirmed)
            m_inputs[slot][frame] = {used, false};
        input.players.push_back(std::move(used));
    }
    const auto sysIt = m_systemCommands.find(frame);
    if (sysIt != m_systemCommands.end())
        input.systemCommands = sysIt->second;
    return input;
}

void RollbackSession::markMisprediction(uint64_t frame)
{
    if (m_pendingRollback == 0 || frame < m_pendingRollback)
        m_pendingRollback = frame;
    ++m_stats.mispredictions;
}

bool RollbackSession::rollbackTo(uint64_t frame)
{
    const uint64_t target = m_currentFrame;
    if (!hasSnapshot(frame - 1))
    {
        MG_LOG_E("rollback target {} beyond snapshot depth (min {})", frame, m_minSnapshotFrame);
        return false;
    }
    m_world         = m_snapshots[snapshotIndex(frame - 1)];
    m_currentFrame  = frame - 1;
    m_pendingRollback = 0;
    ++m_stats.rollbacks;
    const uint64_t resimCount = target - (frame - 1);
    if (resimCount > m_stats.maxDepth)
        m_stats.maxDepth = resimCount;
    for (uint64_t f = frame; f <= target; ++f)
    {
        advanceOneFrame();
        ++m_stats.resimFrames;
    }
    return true;
}

void RollbackSession::advanceOneFrame()
{
    const uint64_t nextFrame = m_currentFrame + 1;
    storeSnapshot(m_currentFrame);
    const FrameInput input = buildInput(nextFrame);
    m_world.tick(input);
    m_currentFrame = nextFrame;
    // 环形快照覆盖的最早帧号，受快照深度与最近一次复位点共同约束（不复位点之后不得回退）。
    const uint64_t ringMin = m_currentFrame >= m_depth ? m_currentFrame - m_depth : 0;
    m_minSnapshotFrame     = std::max(m_minSnapshotFrame, ringMin);
    collectEvents(nextFrame);
}

void RollbackSession::storeSnapshot(uint64_t afterFrame)
{
    m_snapshots[snapshotIndex(afterFrame)] = m_world;
}

bool RollbackSession::hasSnapshot(uint64_t afterFrame) const
{
    return afterFrame < m_currentFrame && afterFrame >= m_minSnapshotFrame;
}

void RollbackSession::collectEvents(uint64_t frame)
{
    auto& keys = m_presentedKeys[snapshotIndex(frame)];
    if (m_presentedKeyFrame[snapshotIndex(frame)] != frame)
    {
        keys.clear();
        m_presentedKeyFrame[snapshotIndex(frame)] = frame;
    }
    for (const auto& event : m_world.ecs().singleton<PresentationEvents>().events)
        if (keys.insert(eventKey(event)).second)
            m_eventsToPresent.push_back(event);
}

uint64_t RollbackSession::eventKey(const PresentationEvent& event)
{
    ByteBuffer buffer;
    BinaryWriter writer(buffer);
    writer.write(event.frame);
    writer.write(event.entity);
    writer.write(event.ordinal);
    writer.write(event.payload);
    writer.finish();
    return xxh64(buffer.data(), buffer.len());
}

void RollbackSession::noteConfirmed(uint32_t slot, uint64_t frame, const PlayerInput& input)
{
    if (frame > m_lastConfirmedFrame[slot])
    {
        m_lastConfirmedFrame[slot] = frame;
        m_lastConfirmedInput[slot] = input;
    }
    if (frame == m_confirmedUpTo[slot] + 1)
    {
        ++m_confirmedUpTo[slot];
        while (true)
        {
            const auto it = m_inputs[slot].find(m_confirmedUpTo[slot] + 1);
            if (it == m_inputs[slot].end() || !it->second.confirmed)
                break;
            ++m_confirmedUpTo[slot];
        }
    }
}

void RollbackSession::resetHistory(uint64_t frame)
{
    m_currentFrame = frame;
    m_localInputs.clear();
    m_systemCommands.clear();
    for (auto& slot : m_inputs)
        slot.clear();
    for (uint32_t slot = 0; slot < m_slotCount; ++slot)
    {
        m_lastConfirmedInput[slot] = PlayerInput{slot, {}};
        m_lastConfirmedFrame[slot] = frame;
        m_confirmedUpTo[slot]      = frame;
    }
    m_pendingRollback = 0;
    m_snapshots[snapshotIndex(frame)] = m_world;
    m_minSnapshotFrame                = frame;
    for (auto& keys : m_presentedKeys)
        keys.clear();
    for (auto& f : m_presentedKeyFrame)
        f = kNoFrame;
    m_eventsToPresent.clear();
}
}  // namespace mugen::sim::rollback
