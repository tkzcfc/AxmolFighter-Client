#pragma once

#include "../summary/SummaryModel.h"
#include "mugen/sim_legacy/World.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// 用旧实现导出效果摘要（附录 A）：按 DrawPolicy 抽样、AI 关闭，逐帧推进并从外部观察状态。
namespace mugen::legacy_cli
{
// 一次导出的参数。
struct ExportParams
{
    // 场景名，写入摘要头部。
    std::string scenario;
    // 房间配置 id。
    int32_t room = 0;
    // 随机种子。
    uint64_t seed = 31;
    // 推进的逻辑帧数。
    uint64_t frames = 700;
    // 暴击策略：min（永不暴击）或 max（必暴击）。
    std::string crit = "min";
};

// 脚本 `frame buff` 行的目标。不写入 FrameInput。
enum class BuffInjectTarget
{
    // 玩家槽位 0。
    Hero,
    // 无槽位、无召唤者的角色里，创建顺序第 1 个。
    Dummy1,
    // 同上，第 2 个。
    Dummy2,
    // 上述全部角色，按创建顺序。
    AllDummies,
};

// 在某帧 tick 之前施加的一条 buff。
struct BuffInject
{
    // 逻辑帧，从 1 起。
    uint64_t frame = 0;
    // 接收者。
    BuffInjectTarget target = BuffInjectTarget::Hero;
    // buff 配置 id。
    int32_t buffId = 0;
};

// 推进世界并生成摘要。世界必须已装配好玩家与房间。
// inputs 是控制脚本；injects 按帧在 tick 前施加，同帧保持调用顺序。
summary::Summary exportSummary(sim_legacy::World& world, const std::map<uint64_t, sim_legacy::FrameInput>& inputs,
                               const ExportParams& params, const std::vector<BuffInject>& injects);
}  // namespace mugen::legacy_cli
