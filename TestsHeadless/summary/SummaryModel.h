#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 效果摘要（golden）数据模型、写出与解析；只依赖标准库，不依赖任何 sim。
namespace mugen::summary
{
// 摘要文件首行魔数。
inline constexpr const char* kMagicLine = "# mugen effect summary v1";

// 一条摘要行：`<帧> <类型> <字段...>`，final 行没有帧。
struct SummaryLine
{
    // 行类型，例如 cast、dmg、final。
    std::string type;
    // 发生帧；final 行为 0。
    int64_t frame = 0;
    // 类型之后的字段（不含帧）。
    std::vector<std::string> fields;
    // 解析自文件时的 1 基行号；程序构造的行为 0。
    int sourceLine = 0;

    // 是否带帧字段。
    bool hasFrame() const { return type != "final"; }
    // 主键：第一个字段（实体键），final 行同样如此。
    const std::string& key() const;
    // 输出稳定文本（不含换行）。
    std::string text() const;
};

// 摘要头部：场景描述与抽样策略。
struct SummaryHeader
{
    // `# scenario` 之后的整段文本，首个词为场景名。
    std::string scenarioInfo;
    // `# draw` 之后的整段文本。
    std::string drawInfo;
    // 场景名（scenarioInfo 的首个词）。
    std::string scenarioName() const;
};

// 完整摘要：头部、行集合与原始文本行（供差异上下文输出）。
struct Summary
{
    SummaryHeader header;
    std::vector<SummaryLine> lines;
    // 原始文件各行（解析时填写，下标 = 行号-1）。
    std::vector<std::string> rawLines;
};

// 整数毫单位（1/1000 逻辑单位）输出为 `-12.345` 形式的十进制文本。
std::string formatMilli(int64_t milli);
// 解析 `formatMilli` 的输出；格式错误返回 false。
bool parseMilli(const std::string& text, int64_t& milli);
// 该行类型是否合法，并返回字段个数范围。
bool lineTypeArity(const std::string& type, size_t& minFields, size_t& maxFields);
// 按帧升序（稳定）排序，final 行放最后并按键排序。
void sortSummary(Summary& summary);
// 写出完整文本，每行以 `\n` 结尾。
std::string writeSummary(const Summary& summary);
// 解析文本；失败返回 false 并在 error 中给出英文原因和行号。
bool parseSummary(const std::string& text, Summary& summary, std::string& error);
}  // namespace mugen::summary
