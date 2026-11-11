#pragma once

#include "../summary/SummaryModel.h"
#include "mugen/sim/World.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// 用新 sim 导出效果摘要（附录 A）：AI 关闭、逐帧推进并从组件状态差分观察。
namespace mugen::sim_cli
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

// 脚本在 tick 之前注入的 Buff。不进 FrameInput。
struct BuffInject
{
    // 逻辑帧，从 1 起。
    uint64_t frame = 0;
    // hero、dummy1、dummy2 或 all-dummies。
    std::string target;
    // Buff 配置 id。
    int32_t buffId = 0;
};

// 推进世界并生成摘要；世界必须已装配好玩家与房间，脚本输入按帧给出。
summary::Summary exportSummary(sim::World& world, const std::map<uint64_t, sim::FrameInput>& inputs,
                               const std::vector<BuffInject>& buffs, const ExportParams& params);
}  // namespace mugen::sim_cli
