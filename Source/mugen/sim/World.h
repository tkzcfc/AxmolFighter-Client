#pragma once
#include "Components.h"
#include "Singletons.h"
#include "WorldSetup.h"
#include "input/FrameInput.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/serialize/BinaryArchive.h"

namespace mugen::sim
{
// 世界使用的组件与单例 ECS 存储类型。
using WorldECS = ECSManager<Components, Singletons>;
// 新 sim 快照格式版本。
inline constexpr uint32_t kWorldSnapshotVersion = 2;
// 持有单场战斗全部可变状态，并按 kSchedule 固定顺序推进一帧。
class World
{
public:
    // 重置 ECS 并按配置初始化本场世界。
    void init(const WorldSetup& setup);
    // 校验并推进一帧确定性输入，执行整张 kSchedule。
    void tick(const FrameInput& input);
    // 将可序列化世界状态写入快照缓冲区。
    void serialize(ByteBuffer& buffer) const;
    // 仅在完整有效快照时替换当前世界状态。
    bool deserialize(ByteBuffer& buffer);
    // 返回独立副本（不携带本帧输入指针）。
    World clone() const;
    // 对规范序列化的世界状态计算 xxh64 哈希。
    uint64_t hash() const;
    // 当前已推进的逻辑帧号。
    uint64_t frame() const { return m_ecs.singleton<WorldClock>().m_frame; }
    // 是否已经初始化。
    bool initialized() const { return m_initialized; }
    // 初始化时传入的配置。
    const WorldSetup& setup() const { return m_setup; }
    WorldECS& ecs() { return m_ecs; }
    const WorldECS& ecs() const { return m_ecs; }
    // 本帧输入，仅在 tick 执行期间有效。
    const FrameInput& input() const { return *m_input; }

private:
    // 初始化时传入的世界配置。
    WorldSetup m_setup;
    // 本场独占的实体、组件与单例。
    WorldECS m_ecs;
    // 是否已经完成初始化。
    bool m_initialized = false;
    // tick 期间指向本帧输入的非拥有指针，不参与快照与克隆。
    const FrameInput* m_input = nullptr;
};
}  // namespace mugen::sim
