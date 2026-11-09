#include "mugen/sim/World.h"
#include "mugen/core/MacroDefinition.h"
#include "mugen/sim/Schedule.h"

namespace mugen::sim
{
// 重置 ECS 并按配置初始化本场世界。
void World::init(const WorldSetup& setup)
{
    const bool validMode = setup.mode == WorldMode::Battle || setup.mode == WorldMode::Town;
    if (!validMode)
    {
        MG_LOG_E("Invalid world mode");
        MG_ASSERT(validMode && "Invalid world mode");
        return;
    }
    m_ecs   = WorldECS{};
    m_setup = setup;
    m_ecs.singleton<RngState>().m_random.seed(m_setup.seed);
    m_initialized = true;
    m_input       = nullptr;
}

// 校验并推进一帧确定性输入。
void World::tick(const FrameInput& input)
{
    const bool nextFrame = m_initialized && input.frame == frame() + 1;
    if (!nextFrame)
    {
        MG_LOG_E("FrameInput frame must be the next world frame");
        MG_ASSERT(nextFrame && "FrameInput frame must be the next world frame");
        return;
    }
    m_input = &input;
    runSchedule(*this, kSchedule);
    m_input = nullptr;
}

// 写入快照：版本、配置、初始化标志与 ECS。
void World::serialize(ByteBuffer& buffer) const
{
    BinaryWriter writer(buffer);
    writer.write(kWorldSnapshotVersion);
    writer.write(m_setup);
    writer.write(m_initialized);
    m_ecs.serialize(buffer);
}

// 仅在完整有效快照通过校验时提交恢复后的世界。
bool World::deserialize(ByteBuffer& buffer)
{
    World restored;
    BinaryReader reader(buffer);
    uint32_t version = 0;
    if (!reader.read(version) || version != kWorldSnapshotVersion || !reader.read(restored.m_setup) ||
        !reader.read(restored.m_initialized) || !restored.m_ecs.deserialize(buffer) ||
        buffer.getPosition() != buffer.getSize() ||
        (restored.m_setup.mode != WorldMode::Battle && restored.m_setup.mode != WorldMode::Town))
        return false;
    *this = std::move(restored);
    return true;
}

// 复制本场全部可序列化状态。
World World::clone() const
{
    World copy   = *this;
    copy.m_input = nullptr;
    return copy;
}

// 对规范序列化的世界状态计算 xxh64 哈希。
uint64_t World::hash() const
{
    ByteBuffer buffer;
    serialize(buffer);
    return xxh64(buffer.data(), buffer.len());
}
}  // namespace mugen::sim
