#pragma once
#include "Components.h"
#include "Singletons.h"
#include "WorldSetup.h"
#include "DrawPolicy.h"
#include "input/FrameInput.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/serialize/BinaryArchive.h"
#include <string>
#include <vector>

namespace mugen::sim
{
// 同步触发链的重入状态。不进快照、哈希与克隆。
struct BuffDispatch
{
    // 当前链深度。
    int32_t m_depth = 0;
    // 本条链已执行的（实例序号，触发点）。
    std::vector<uint64_t> m_seen;
};
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
    // 返回可序列化状态的独立副本。抽样策略、追踪行与本帧输入指针不复制。
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
    // 挂载抽样策略，传入空指针即恢复使用世界随机数。
    void setDrawPolicy(const DrawPolicy* policy) { m_drawPolicy = policy; }
    // 返回当前抽样策略，未设置时为空。
    const DrawPolicy* drawPolicy() const { return m_drawPolicy; }
    // DebugSink 写入的追踪行（非序列化）。DebugSink 每帧开始时清空，摘要导出在 tick 之后读取。
    const std::vector<std::string>& traceLines() const { return m_traceLines; }
    // 追加一条追踪行（DebugSink 调用）。
    void appendTrace(std::string line) { m_traceLines.push_back(std::move(line)); }
    // 清空追踪行（DebugSink 调用）。
    void clearTraceLines() { m_traceLines.clear(); }
    // 同步触发链重入状态。不进快照。
    BuffDispatch& buffChain() { return m_buffChain; }

private:
    // 初始化时传入的世界配置。
    WorldSetup m_setup;
    // 本场独占的实体、组件与单例。
    WorldECS m_ecs;
    // 是否已经完成初始化。
    bool m_initialized = false;
    // tick 期间指向本帧输入的非拥有指针，不参与快照与克隆。
    const FrameInput* m_input = nullptr;
    // 抽样策略，非拥有、不参与快照/哈希/克隆。
    const DrawPolicy* m_drawPolicy = nullptr;
    // DebugSink 追踪行。不参与快照、哈希与克隆。
    std::vector<std::string> m_traceLines;
    // 同步触发链。不进快照、哈希与克隆。
    BuffDispatch m_buffChain;
};
}  // namespace mugen::sim
