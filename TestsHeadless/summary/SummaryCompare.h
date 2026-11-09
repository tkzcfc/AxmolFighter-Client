#pragma once

#include "SummaryModel.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

// 效果摘要比对：按（行类型，主键）分组、组内按序对齐，逐行检查容差规则。
namespace mugen::summary
{
// 比对容差。
struct CompareOptions
{
    // 帧差容差（帧）。
    int64_t frameTolerance = 1;
    // 位置容差（毫单位，2000 = 2 个逻辑单位）。
    int64_t positionToleranceMilli = 2000;
    // 顿帧帧数容差。
    int64_t stopFramesTolerance = 1;
};

// 已知偏差条目：匹配的差异只告警不失败。
struct KnownDeviation
{
    // 行类型，`*` 匹配任意。
    std::string type;
    // 场景名模式，支持 `*` 通配。
    std::string scenario;
    // 主键模式（`key=` 前缀的可选列），为空则不限。
    std::string key;
    // 原因文本。
    std::string reason;
};

// 一处差异。
struct Difference
{
    enum class Kind
    {
        // 头部不一致。
        Header,
        // 对齐后字段或帧超出容差。
        Mismatch,
        // 黄金中有而实际缺失。
        Missing,
        // 实际中多出。
        Extra,
    };
    Kind kind = Kind::Mismatch;
    // 黄金/实际的行下标（lines 内下标），无则为 -1。
    int goldenIndex = -1;
    int actualIndex = -1;
    // 用于挑选“第一个差异”的帧。
    int64_t frame = 0;
    // 差异说明。
    std::string message;
    // 命中已知偏差。
    bool known = false;
    // 命中的偏差原因。
    std::string reason;
};

// 比对结果。
struct CompareReport
{
    std::vector<Difference> differences;
    size_t goldenLines = 0;
    size_t actualLines = 0;
    // 成功对齐的行数（含帧有偏移的）。
    size_t aligned = 0;
    // 帧有偏移但在容差内的行数。
    size_t shifted = 0;
    size_t errors   = 0;
    size_t warnings = 0;
    // 是否通过（无错误）。
    bool passed() const { return errors == 0; }
};

// 解析已知偏差文本；每行 `<类型|*> <场景模式> [key=<模式>] <原因>`，`#` 开头为注释。
bool parseKnownDeviations(const std::string& text, std::vector<KnownDeviation>& out, std::string& error);
// 比对两份已解析的摘要。
CompareReport compareSummaries(const Summary& golden, const Summary& actual,
                               const std::vector<KnownDeviation>& deviations, const CompareOptions& options = {});
// 输出第一个差异（前后各 5 行上下文）与汇总计数。
void writeReport(const CompareReport& report, const Summary& golden, const Summary& actual, std::ostream& out);
// 读取文件、比对并输出报告；通过返回 0，否则（含读文件失败）返回 1。deviationsPath 为空表示无偏差表。
int runCompareSummary(const std::string& goldenPath, const std::string& actualPath, const std::string& deviationsPath,
                      std::ostream& out);
}  // namespace mugen::summary
