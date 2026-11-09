#pragma once

#include "../summary/SummaryModel.h"
#include "mugen/sim_legacy/World.h"
#include <cstdint>
#include <map>
#include <string>

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

// 推进世界并生成摘要；世界必须已装配好玩家与房间，脚本输入按帧给出。
summary::Summary exportSummary(sim_legacy::World& world, const std::map<uint64_t, sim_legacy::FrameInput>& inputs,
                               const ExportParams& params);
}  // namespace mugen::legacy_cli
