#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace effects_trace
{
// 事实级对比结果：帧偏移只写入报告，不改变 matched。
struct EffectsDiff
{
    bool matched = false;
    std::string report;
};

struct EffectEvent
{
    uint64_t frame = 0;
    std::string entity;
    std::string kind;
    std::string line;
};

namespace
{
constexpr std::string_view kKinds[] = {"cast", "castend", "dmg", "buff+", "buff-", "spawn", "death"};

bool parseEvent(const std::string& line, std::string& kind, std::string& entity)
{
    std::istringstream row(line);
    std::string verb;
    if (!(row >> verb))
        return false;
    if (verb == "buff")
    {
        std::string sign;
        if (!(row >> sign >> entity) || (sign != "+" && sign != "-"))
            return false;
        kind = verb + sign;
        return true;
    }
    if (verb != "cast" && verb != "castend" && verb != "dmg" && verb != "spawn" && verb != "death")
        return false;
    if (!(row >> entity))
        return false;
    kind = verb;
    return true;
}

bool allowed(const std::string& line, const std::vector<std::regex>& patterns)
{
    for (const auto& pattern : patterns)
        if (std::regex_search(line, pattern))
            return true;
    return false;
}

uint64_t frameNumber(std::string_view line)
{
    uint64_t frame = 0;
    for (const char digit : line.substr(6))
    {
        if (digit < '0' || digit > '9')
            break;
        frame = frame * 10 + static_cast<uint64_t>(digit - '0');
    }
    return frame;
}

std::vector<std::regex> compileAllow(const std::vector<std::string>& patterns)
{
    std::vector<std::regex> compiled;
    compiled.reserve(patterns.size());
    for (const auto& pattern : patterns)
    {
        try
        {
            compiled.emplace_back(pattern);
        }
        catch (const std::regex_error&)
        {
            throw std::invalid_argument("Invalid allow pattern: " + pattern);
        }
    }
    return compiled;
}

// 事实行写在当帧 frame 行之前，帧号取自下一条 frame 行。
std::vector<EffectEvent> parseEffects(const std::string& text, const std::vector<std::regex>& patterns)
{
    std::vector<EffectEvent> pending;
    std::vector<EffectEvent> events;
    uint64_t lastFrame = 0;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.rfind("frame ", 0) == 0)
        {
            const uint64_t frame = frameNumber(line);
            for (auto& event : pending)
                event.frame = frame;
            events.insert(events.end(), pending.begin(), pending.end());
            pending.clear();
            lastFrame = frame;
            continue;
        }
        std::string kind;
        std::string entity;
        if (!parseEvent(line, kind, entity) || allowed(line, patterns))
            continue;
        pending.push_back(EffectEvent{0, std::move(entity), std::move(kind), line});
    }
    for (auto& event : pending)
    {
        event.frame = lastFrame;
        events.push_back(std::move(event));
    }
    return events;
}

std::map<std::string, std::vector<EffectEvent>> byEntity(const std::vector<EffectEvent>& events)
{
    std::map<std::string, std::vector<EffectEvent>> grouped;
    for (const auto& event : events)
        grouped[event.entity].push_back(event);
    return grouped;
}

std::map<std::string, int> kindCounts(const std::vector<EffectEvent>& events)
{
    std::map<std::string, int> counts;
    for (const auto kind : kKinds)
        counts.emplace(std::string(kind), 0);
    for (const auto& event : events)
        ++counts[event.kind];
    return counts;
}

void appendContext(std::ostringstream& out, const char* side, const std::vector<EffectEvent>& events, size_t index)
{
    const size_t begin = index > 2 ? index - 2 : 0;
    for (size_t cursor = begin; cursor < index && cursor < events.size(); ++cursor)
        out << "  " << side << '[' << cursor << "] f" << events[cursor].frame << ": " << events[cursor].line << '\n';
    if (index >= events.size())
        out << "  " << side << '[' << index << "]: <missing>\n";
    else
        out << "  " << side << '[' << index << "] f" << events[index].frame << ": " << events[index].line << '\n';
}

void appendOffsets(std::ostringstream& out, const std::string& entity, const std::vector<EffectEvent>& left,
                    const std::vector<EffectEvent>& right, size_t count)
{
    int nonzero = 0;
    int64_t maxAbs = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const int64_t delta = static_cast<int64_t>(right[index].frame) - static_cast<int64_t>(left[index].frame);
        const int64_t magnitude = delta < 0 ? -delta : delta;
        if (magnitude > maxAbs)
            maxAbs = magnitude;
        if (delta != 0)
            ++nonzero;
    }
    out << "offsets " << entity << " matched " << count << " nonzero " << nonzero << " max " << maxAbs << '\n';
    int printed = 0;
    for (size_t index = 0; index < count && printed < 8; ++index)
    {
        const int64_t delta = static_cast<int64_t>(right[index].frame) - static_cast<int64_t>(left[index].frame);
        if (delta == 0)
            continue;
        out << "  [" << index << "] a=" << left[index].frame << " b=" << right[index].frame << " b-a=" << delta << '\n';
        ++printed;
    }
}
}  // namespace

// 丢弃帧号与非事实行。各实体事实序列必须一致，再比较事实种类总数。
inline EffectsDiff diffEffects(const std::string& leftText, const std::string& rightText,
                               const std::vector<std::string>& allowPatterns)
{
    const auto patterns = compileAllow(allowPatterns);
    const auto left = parseEffects(leftText, patterns);
    const auto right = parseEffects(rightText, patterns);
    const auto leftGroups = byEntity(left);
    const auto rightGroups = byEntity(right);
    std::ostringstream out;
    bool matched = true;
    std::map<std::string, int> entities;
    for (const auto& item : leftGroups)
        entities.emplace(item.first, 0);
    for (const auto& item : rightGroups)
        entities.emplace(item.first, 0);
    for (const auto& item : entities)
    {
        const auto& entity = item.first;
        static const std::vector<EffectEvent> kNone;
        const auto leftIt = leftGroups.find(entity);
        const auto rightIt = rightGroups.find(entity);
        const auto& leftEvents = leftIt == leftGroups.end() ? kNone : leftIt->second;
        const auto& rightEvents = rightIt == rightGroups.end() ? kNone : rightIt->second;
        const size_t shared = std::min(leftEvents.size(), rightEvents.size());
        size_t index = 0;
        for (; index < shared; ++index)
            if (leftEvents[index].line != rightEvents[index].line)
                break;
        if (index == leftEvents.size() && index == rightEvents.size())
        {
            appendOffsets(out, entity, leftEvents, rightEvents, shared);
            continue;
        }
        matched = false;
        out << "mismatch " << entity << " event " << index << '\n';
        appendContext(out, "a", leftEvents, index);
        appendContext(out, "b", rightEvents, index);
        if (index > 0)
            appendOffsets(out, entity, leftEvents, rightEvents, index);
    }
    const auto leftCounts = kindCounts(left);
    const auto rightCounts = kindCounts(right);
    for (const auto kind : kKinds)
    {
        const auto key = std::string(kind);
        const int leftCount = leftCounts.at(key);
        const int rightCount = rightCounts.at(key);
        out << "counts " << key << " a=" << leftCount << " b=" << rightCount << '\n';
        if (leftCount != rightCount)
            matched = false;
    }
    EffectsDiff diff;
    diff.matched = matched;
    diff.report = out.str();
    return diff;
}
}  // namespace effects_trace
