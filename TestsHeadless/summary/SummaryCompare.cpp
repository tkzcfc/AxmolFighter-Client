#include "SummaryCompare.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <ostream>
#include <sstream>
#include <utility>

namespace mugen::summary
{
namespace
{
// 支持 `*` 的简单通配匹配。
bool globMatch(const std::string& pattern, const std::string& text)
{
    size_t p = 0, t = 0, star = std::string::npos, mark = 0;
    while (t < text.size())
    {
        if (p < pattern.size() && pattern[p] == '*')
        {
            star = p++;
            mark = t;
        }
        else if (p < pattern.size() && pattern[p] == text[t])
        {
            ++p;
            ++t;
        }
        else if (star != std::string::npos)
        {
            p = star + 1;
            t = ++mark;
        }
        else
            return false;
    }
    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}

int64_t absDiff(int64_t a, int64_t b)
{
    return a > b ? a - b : b - a;
}

// 检查位置三元组（毫单位文本）是否在容差内；解析失败视为不等。
bool positionsClose(const std::vector<std::string>& a, const std::vector<std::string>& b, size_t offset,
                    int64_t tolerance, std::string& why)
{
    static const char* kAxis[] = {"x", "y", "z"};
    for (size_t i = 0; i < 3; ++i)
    {
        int64_t va = 0, vb = 0;
        if (!parseMilli(a[offset + i], va) || !parseMilli(b[offset + i], vb))
        {
            why = std::string("bad number on axis ") + kAxis[i];
            return false;
        }
        if (absDiff(va, vb) > tolerance)
        {
            why = std::string(kAxis[i]) + " " + a[offset + i] + " vs " + b[offset + i] + " exceeds " +
                  formatMilli(tolerance);
            return false;
        }
    }
    return true;
}

// 逐类型检查字段规则；通过返回空串，否则返回原因。帧差由调用方检查。
std::string checkFields(const SummaryLine& g, const SummaryLine& a, const CompareOptions& options)
{
    const auto exact = [&](size_t from, size_t to) -> std::string {
        for (size_t i = from; i < to && i < g.fields.size() && i < a.fields.size(); ++i)
            if (g.fields[i] != a.fields[i])
                return "field " + std::to_string(i + 1) + " '" + g.fields[i] + "' vs '" + a.fields[i] + "'";
        return {};
    };
    if (g.type == "stop")
    {
        if (auto why = exact(0, 1); !why.empty())
            return why;
        int64_t gf = 0, af = 0;
        try
        {
            gf = std::stoll(g.fields[1]);
            af = std::stoll(a.fields[1]);
        }
        catch (...)
        {
            return "bad stop frame count";
        }
        if (absDiff(gf, af) > options.stopFramesTolerance)
            return "stop frames " + g.fields[1] + " vs " + a.fields[1];
        return {};
    }
    if (g.type == "disp+" || g.type == "disp-")
    {
        if (auto why = exact(0, 2); !why.empty())
            return why;
        if (g.fields.size() == 5 && a.fields.size() == 5)
        {
            std::string why;
            if (!positionsClose(g.fields, a.fields, 2, options.positionToleranceMilli, why))
                return "end point " + why;
        }
        return {};
    }
    if (g.type == "final")
    {
        if (g.fields[1] != a.fields[1])
            return "hp " + g.fields[1] + " vs " + a.fields[1];
        std::string why;
        if (!positionsClose(g.fields, a.fields, 2, options.positionToleranceMilli, why))
            return "position " + why;
        return {};
    }
    return exact(0, std::max(g.fields.size(), a.fields.size()));
}

const KnownDeviation* findDeviation(const std::vector<KnownDeviation>& deviations, const std::string& type,
                                    const std::string& scenario, const std::string& key)
{
    for (const auto& d : deviations)
        if ((d.type == "*" || d.type == type) && globMatch(d.scenario, scenario) &&
            (d.key.empty() || globMatch(d.key, key)))
            return &d;
    return nullptr;
}
}  // namespace

bool parseKnownDeviations(const std::string& text, std::vector<KnownDeviation>& out, std::string& error)
{
    out.clear();
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw))
    {
        ++lineNo;
        if (!raw.empty() && raw.back() == '\r')
            raw.pop_back();
        std::istringstream row(raw);
        KnownDeviation entry;
        if (!(row >> entry.type) || entry.type[0] == '#')
            continue;
        if (!(row >> entry.scenario))
        {
            error = "line " + std::to_string(lineNo) + ": expected '<type|*> <scenario> [key=<pattern>] <reason>'";
            return false;
        }
        std::string word;
        std::string rest;
        bool first = true;
        while (row >> word)
        {
            if (first && word.rfind("key=", 0) == 0)
                entry.key = word.substr(4);
            else
                rest += (rest.empty() ? "" : " ") + word;
            first = false;
        }
        if (rest.empty())
        {
            error = "line " + std::to_string(lineNo) + ": missing reason";
            return false;
        }
        entry.reason = rest;
        out.push_back(std::move(entry));
    }
    return true;
}

CompareReport compareSummaries(const Summary& golden, const Summary& actual,
                               const std::vector<KnownDeviation>& deviations, const CompareOptions& options)
{
    CompareReport report;
    report.goldenLines = golden.lines.size();
    report.actualLines = actual.lines.size();
    const std::string scenario = actual.header.scenarioName().empty() ? golden.header.scenarioName()
                                                                      : actual.header.scenarioName();
    const auto addDiff = [&](Difference diff, const std::string& type, const std::string& key) {
        if (const auto* dev = findDeviation(deviations, type, scenario, key))
        {
            diff.known  = true;
            diff.reason = dev->reason;
            ++report.warnings;
        }
        else
            ++report.errors;
        report.differences.push_back(std::move(diff));
    };

    if (golden.header.drawInfo != actual.header.drawInfo)
    {
        Difference diff;
        diff.kind    = Difference::Kind::Header;
        diff.frame   = -1;
        diff.message = "draw policy differs: '" + golden.header.drawInfo + "' vs '" + actual.header.drawInfo + "'";
        addDiff(std::move(diff), "*", "*");
    }

    using GroupKey = std::pair<std::string, std::string>;
    std::map<GroupKey, std::vector<int>> goldenGroups, actualGroups;
    for (size_t i = 0; i < golden.lines.size(); ++i)
        goldenGroups[{golden.lines[i].type, golden.lines[i].key()}].push_back(static_cast<int>(i));
    for (size_t i = 0; i < actual.lines.size(); ++i)
        actualGroups[{actual.lines[i].type, actual.lines[i].key()}].push_back(static_cast<int>(i));

    std::map<GroupKey, bool> seen;
    for (const auto& [key, _] : goldenGroups)
        seen[key] = true;
    for (const auto& [key, _] : actualGroups)
        seen[key] = true;
    for (const auto& [key, _] : seen)
    {
        static const std::vector<int> kNone;
        const auto gi          = goldenGroups.find(key);
        const auto ai          = actualGroups.find(key);
        const auto& goldenIds  = gi == goldenGroups.end() ? kNone : gi->second;
        const auto& actualIds  = ai == actualGroups.end() ? kNone : ai->second;
        const size_t common    = std::min(goldenIds.size(), actualIds.size());
        for (size_t n = 0; n < common; ++n)
        {
            const auto& g = golden.lines[static_cast<size_t>(goldenIds[n])];
            const auto& a = actual.lines[static_cast<size_t>(actualIds[n])];
            std::string why;
            if (g.hasFrame() && absDiff(g.frame, a.frame) > options.frameTolerance)
                why = "frame " + std::to_string(g.frame) + " vs " + std::to_string(a.frame);
            else
                why = checkFields(g, a, options);
            if (why.empty())
            {
                ++report.aligned;
                if (g.hasFrame() && g.frame != a.frame)
                    ++report.shifted;
                continue;
            }
            Difference diff;
            diff.kind        = Difference::Kind::Mismatch;
            diff.goldenIndex = goldenIds[n];
            diff.actualIndex = actualIds[n];
            diff.frame       = g.hasFrame() ? g.frame : INT64_MAX;
            diff.message     = "mismatch (" + why + ")";
            addDiff(std::move(diff), g.type, g.key());
        }
        for (size_t n = common; n < goldenIds.size(); ++n)
        {
            const auto& g    = golden.lines[static_cast<size_t>(goldenIds[n])];
            Difference diff;
            diff.kind        = Difference::Kind::Missing;
            diff.goldenIndex = goldenIds[n];
            diff.frame       = g.hasFrame() ? g.frame : INT64_MAX;
            diff.message     = "missing in actual";
            addDiff(std::move(diff), g.type, g.key());
        }
        for (size_t n = common; n < actualIds.size(); ++n)
        {
            const auto& a    = actual.lines[static_cast<size_t>(actualIds[n])];
            Difference diff;
            diff.kind        = Difference::Kind::Extra;
            diff.actualIndex = actualIds[n];
            diff.frame       = a.hasFrame() ? a.frame : INT64_MAX;
            diff.message     = "extra in actual";
            addDiff(std::move(diff), a.type, a.key());
        }
    }
    return report;
}

namespace
{
// 输出 [center-5, center+5] 行，中心行加 `>>` 标记；center 为 0 表示无中心。
void writeContext(std::ostream& out, const char* title, const Summary& summary, int centerLine)
{
    out << title << ":\n";
    if (centerLine <= 0)
    {
        out << "  (no line)\n";
        return;
    }
    const int total = static_cast<int>(summary.rawLines.size());
    for (int n = std::max(1, centerLine - 5); n <= std::min(total, centerLine + 5); ++n)
        out << (n == centerLine ? "  >> " : "     ") << n << ": " << summary.rawLines[static_cast<size_t>(n - 1)]
            << '\n';
}
// 无对应行时，取帧不小于 frame 的第一行作为定位参考。
int anchorLine(const Summary& summary, int64_t frame)
{
    for (const auto& line : summary.lines)
        if (line.hasFrame() && line.frame >= frame)
            return line.sourceLine;
    return summary.lines.empty() ? 0 : summary.lines.back().sourceLine;
}
const char* kindName(Difference::Kind kind)
{
    switch (kind)
    {
    case Difference::Kind::Header:
        return "header";
    case Difference::Kind::Mismatch:
        return "mismatch";
    case Difference::Kind::Missing:
        return "missing";
    default:
        return "extra";
    }
}
}  // namespace

void writeReport(const CompareReport& report, const Summary& golden, const Summary& actual, std::ostream& out)
{
    const Difference* first = nullptr;
    for (const auto& diff : report.differences)
        if (!diff.known && (!first || diff.frame < first->frame))
            first = &diff;
    if (first)
    {
        out << "first difference: " << first->message << " (frame " << (first->frame == INT64_MAX ? -1 : first->frame)
            << ")\n";
        int goldenLine = first->goldenIndex >= 0 ? golden.lines[static_cast<size_t>(first->goldenIndex)].sourceLine : 0;
        int actualLine = first->actualIndex >= 0 ? actual.lines[static_cast<size_t>(first->actualIndex)].sourceLine : 0;
        if (first->kind == Difference::Kind::Header)
        {
            out << "  golden: " << golden.header.drawInfo << "\n  actual: " << actual.header.drawInfo << '\n';
        }
        else
        {
            if (goldenLine == 0)
                goldenLine = anchorLine(golden, first->frame);
            if (actualLine == 0)
                actualLine = anchorLine(actual, first->frame);
            writeContext(out, "golden", golden, goldenLine);
            writeContext(out, "actual", actual, actualLine);
        }
    }
    for (const auto& diff : report.differences)
        if (diff.known)
        {
            const auto& line = diff.goldenIndex >= 0 ? golden.lines[static_cast<size_t>(diff.goldenIndex)]
                                                     : actual.lines[static_cast<size_t>(diff.actualIndex)];
            out << "warning (known deviation: " << diff.reason << "): " << kindName(diff.kind) << ' ' << line.text()
                << '\n';
        }
    size_t byKind[4] = {};
    for (const auto& diff : report.differences)
        if (!diff.known)
            ++byKind[static_cast<size_t>(diff.kind)];
    out << "summary: golden=" << report.goldenLines << " actual=" << report.actualLines
        << " aligned=" << report.aligned << " shifted=" << report.shifted << " errors=" << report.errors
        << " (mismatch=" << byKind[static_cast<size_t>(Difference::Kind::Mismatch)]
        << " missing=" << byKind[static_cast<size_t>(Difference::Kind::Missing)]
        << " extra=" << byKind[static_cast<size_t>(Difference::Kind::Extra)]
        << " header=" << byKind[static_cast<size_t>(Difference::Kind::Header)] << ") warnings=" << report.warnings
        << '\n';
    out << (report.passed() ? "compare-summary: OK" : "compare-summary: FAILED") << '\n';
}

namespace
{
bool readFile(const std::string& path, std::string& text)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}
}  // namespace

int runCompareSummary(const std::string& goldenPath, const std::string& actualPath, const std::string& deviationsPath,
                      std::ostream& out)
{
    std::string goldenText, actualText, deviationText, error;
    if (!readFile(goldenPath, goldenText))
    {
        out << "Cannot open golden summary: " << goldenPath << '\n';
        return 1;
    }
    if (!readFile(actualPath, actualText))
    {
        out << "Cannot open actual summary: " << actualPath << '\n';
        return 1;
    }
    std::vector<KnownDeviation> deviations;
    if (!deviationsPath.empty())
    {
        if (!readFile(deviationsPath, deviationText))
        {
            out << "Cannot open known deviations: " << deviationsPath << '\n';
            return 1;
        }
        if (!parseKnownDeviations(deviationText, deviations, error))
        {
            out << "Invalid known deviations: " << error << '\n';
            return 1;
        }
    }
    Summary golden, actual;
    if (!parseSummary(goldenText, golden, error))
    {
        out << "Invalid golden summary: " << error << '\n';
        return 1;
    }
    if (!parseSummary(actualText, actual, error))
    {
        out << "Invalid actual summary: " << error << '\n';
        return 1;
    }
    const auto report = compareSummaries(golden, actual, deviations);
    writeReport(report, golden, actual, out);
    return report.passed() ? 0 : 1;
}
}  // namespace mugen::summary
