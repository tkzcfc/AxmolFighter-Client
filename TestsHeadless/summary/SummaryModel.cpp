#include "SummaryModel.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace mugen::summary
{
namespace
{
struct TypeArity
{
    const char* name;
    size_t minFields;
    size_t maxFields;
};
// 各行类型的字段个数（不含帧）。
constexpr TypeArity kArity[] = {
    {"cast", 3, 3},    {"castend", 3, 3}, {"action", 2, 2}, {"actionend", 2, 2}, {"spawn", 2, 2},
    {"hit", 4, 4},     {"dmg", 5, 5},     {"hp", 3, 3},     {"stop", 2, 2},      {"state", 3, 3},
    {"buff+", 2, 2},   {"buff-", 2, 2},   {"disp+", 2, 5},  {"disp-", 2, 5},     {"death", 1, 1},
    {"final", 6, 6},
    // S4 房间流程：状态切换、胜负、警告、波次、复活、KO、障碍/物品/传送门。
    {"roomstate", 1, 1}, {"victory", 1, 1}, {"failure", 1, 1}, {"warning", 1, 1}, {"wave", 1, 1},
    {"revive", 1, 1},    {"ko", 1, 1},      {"obstacle", 2, 2}, {"goods", 2, 2},  {"portal", 2, 2},
};

std::vector<std::string> splitWords(const std::string& text)
{
    std::vector<std::string> words;
    std::istringstream in(text);
    std::string word;
    while (in >> word)
        words.push_back(word);
    return words;
}
bool parseInt(const std::string& text, int64_t& value)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    value     = std::strtoll(text.c_str(), &end, 10);
    return end == text.c_str() + text.size();
}
}  // namespace

const std::string& SummaryLine::key() const
{
    static const std::string kEmpty;
    return fields.empty() ? kEmpty : fields.front();
}

std::string SummaryLine::text() const
{
    std::string out;
    if (hasFrame())
        out = std::to_string(frame) + ' ';
    out += type;
    for (const auto& field : fields)
        out += ' ' + field;
    return out;
}

std::string SummaryHeader::scenarioName() const
{
    const auto end = scenarioInfo.find(' ');
    return end == std::string::npos ? scenarioInfo : scenarioInfo.substr(0, end);
}

std::string formatMilli(int64_t milli)
{
    const bool negative = milli < 0;
    const uint64_t mag  = negative ? static_cast<uint64_t>(-(milli + 1)) + 1 : static_cast<uint64_t>(milli);
    std::string frac    = std::to_string(mag % 1000);
    frac.insert(0, 3 - frac.size(), '0');
    return (negative ? "-" : "") + std::to_string(mag / 1000) + '.' + frac;
}

bool parseMilli(const std::string& text, int64_t& milli)
{
    size_t pos          = 0;
    const bool negative = !text.empty() && text[0] == '-';
    if (negative)
        pos = 1;
    const auto dot = text.find('.', pos);
    if (dot == std::string::npos || dot == pos || text.size() - dot - 1 != 3)
        return false;
    int64_t whole = 0, frac = 0;
    if (!parseInt(text.substr(pos, dot - pos), whole) || !parseInt(text.substr(dot + 1), frac) || whole < 0 || frac < 0)
        return false;
    const int64_t value = whole * 1000 + frac;
    milli               = negative ? -value : value;
    return true;
}

bool lineTypeArity(const std::string& type, size_t& minFields, size_t& maxFields)
{
    for (const auto& row : kArity)
        if (type == row.name)
        {
            minFields = row.minFields;
            maxFields = row.maxFields;
            return true;
        }
    return false;
}

void sortSummary(Summary& summary)
{
    std::stable_sort(summary.lines.begin(), summary.lines.end(), [](const SummaryLine& a, const SummaryLine& b) {
        if (a.hasFrame() != b.hasFrame())
            return a.hasFrame();
        if (!a.hasFrame())
            return a.key() < b.key();
        return a.frame < b.frame;
    });
}

std::string writeSummary(const Summary& summary)
{
    std::string out = std::string(kMagicLine) + '\n';
    out += "# scenario " + summary.header.scenarioInfo + '\n';
    out += "# draw " + summary.header.drawInfo + '\n';
    for (const auto& line : summary.lines)
        out += line.text() + '\n';
    return out;
}

bool parseSummary(const std::string& text, Summary& summary, std::string& error)
{
    summary = Summary{};
    std::istringstream in(text);
    std::string raw;
    int lineNo       = 0;
    bool sawMagic    = false;
    while (std::getline(in, raw))
    {
        ++lineNo;
        if (!raw.empty() && raw.back() == '\r')
            raw.pop_back();
        summary.rawLines.push_back(raw);
        if (raw.empty())
            continue;
        const auto fail = [&](const std::string& why) {
            error = "line " + std::to_string(lineNo) + ": " + why;
            return false;
        };
        if (raw[0] == '#')
        {
            if (raw == kMagicLine)
                sawMagic = true;
            else if (raw.rfind("# scenario ", 0) == 0)
                summary.header.scenarioInfo = raw.substr(11);
            else if (raw.rfind("# draw ", 0) == 0)
                summary.header.drawInfo = raw.substr(7);
            continue;
        }
        if (!sawMagic)
            return fail("missing summary magic header");
        auto words = splitWords(raw);
        SummaryLine line;
        line.sourceLine = lineNo;
        size_t first    = 0;
        if (words[0] == "final")
        {
            line.type = "final";
            first     = 1;
        }
        else
        {
            if (words.size() < 2 || !parseInt(words[0], line.frame))
                return fail("expected '<frame> <type> ...'");
            line.type = words[1];
            first     = 2;
        }
        size_t minFields = 0, maxFields = 0;
        if (!lineTypeArity(line.type, minFields, maxFields))
            return fail("unknown line type '" + line.type + "'");
        const size_t count = words.size() - first;
        if (count < minFields || count > maxFields)
            return fail("wrong field count for '" + line.type + "'");
        line.fields.assign(words.begin() + static_cast<std::ptrdiff_t>(first), words.end());
        summary.lines.push_back(std::move(line));
    }
    if (!sawMagic)
    {
        error = "missing summary magic header";
        return false;
    }
    return true;
}
}  // namespace mugen::summary
