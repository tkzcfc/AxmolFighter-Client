#include "../SummaryCompare.h"

#include "doctest.h"
#include <sstream>

namespace
{
using namespace mugen::summary;

// 一份覆盖各行类型的样例摘要文本。
const char* kSample =
    "# mugen effect summary v1\n"
    "# scenario unit_case room 1 seed 31 ai off\n"
    "# draw crit=min float=mid dodge=never prob=pass choice=first\n"
    "10 cast role:101#1 1001 A\n"
    "12 action role:101#1 atk_a\n"
    "14 spawn role:101#1 fx:5001#1\n"
    "20 hit fx:5001#1 role:9001#1 77 hit\n"
    "20 dmg role:101#1 role:9001#1 5001 120 0\n"
    "21 hp role:9001#1 -8 dot\n"
    "22 stop role:9001#1 6\n"
    "23 state role:9001#1 Idle Hit\n"
    "24 buff+ role:9001#1 301\n"
    "30 disp+ role:101#1 12\n"
    "40 disp- role:101#1 12 10.500 0.000 -2.250\n"
    "41 buff- role:9001#1 301\n"
    "50 actionend role:101#1 atk_a\n"
    "52 castend role:101#1 1001 done\n"
    "60 death role:9001#1\n"
    "final role:101#1 900 10.500 0.000 -2.250 Idle\n"
    "final role:9001#1 0 30.000 0.000 1.000 Death\n";

Summary parse(const std::string& text)
{
    Summary summary;
    std::string error;
    REQUIRE_MESSAGE(parseSummary(text, summary, error), error);
    return summary;
}
// 把 from 首次出现处替换为 to，作为注入偏差的手段。
std::string patched(const std::string& text, const std::string& from, const std::string& to)
{
    const auto pos = text.find(from);
    REQUIRE(pos != std::string::npos);
    return text.substr(0, pos) + to + text.substr(pos + from.size());
}
CompareReport compare(const std::string& actualText, const std::vector<KnownDeviation>& deviations = {})
{
    return compareSummaries(parse(kSample), parse(actualText), deviations);
}
}  // namespace

TEST_CASE("summary parse and write round trip")
{
    const auto summary = parse(kSample);
    CHECK(summary.header.scenarioName() == "unit_case");
    CHECK(summary.lines.size() == 17);
    CHECK(writeSummary(summary) == kSample);
    // 写出后再次解析得到相同文本。
    CHECK(writeSummary(parse(writeSummary(summary))) == kSample);
}

TEST_CASE("summary sort keeps frame order and final last")
{
    auto summary = parse(kSample);
    std::swap(summary.lines.front(), summary.lines.back());
    sortSummary(summary);
    CHECK(summary.lines.front().frame == 10);
    CHECK(summary.lines.back().type == "final");
}

TEST_CASE("summary milli formatting")
{
    CHECK(formatMilli(0) == "0.000");
    CHECK(formatMilli(-2250) == "-2.250");
    CHECK(formatMilli(-5) == "-0.005");
    int64_t value = 0;
    CHECK(parseMilli("-0.005", value));
    CHECK(value == -5);
    CHECK_FALSE(parseMilli("1.5", value));
}

TEST_CASE("summary parser rejects malformed input")
{
    Summary summary;
    std::string error;
    CHECK_FALSE(parseSummary("10 cast a 1 A\n", summary, error));
    CHECK_FALSE(parseSummary(std::string(kMagicLine) + "\n10 bogus x\n", summary, error));
    CHECK_FALSE(parseSummary(std::string(kMagicLine) + "\n10 cast a\n", summary, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("summary self compare passes")
{
    const auto report = compare(kSample);
    CHECK(report.passed());
    CHECK(report.errors == 0);
    CHECK(report.warnings == 0);
    CHECK(report.aligned == 17);
}

TEST_CASE("summary compare accepts tolerated differences")
{
    // 帧 ±1、位置 ≤2 单位、顿帧帧数 ±1 均通过。
    std::string text = patched(kSample, "10 cast", "11 cast");
    text             = patched(text, "20 dmg", "19 dmg");
    text             = patched(text, "10.500 0.000 -2.250 Idle", "12.500 0.000 -2.250 Idle");
    text             = patched(text, "stop role:9001#1 6", "stop role:9001#1 7");
    text             = patched(text, "12 10.500 0.000 -2.250", "12 11.900 0.000 -2.250");
    const auto report = compare(text);
    CHECK(report.passed());
    CHECK(report.shifted == 2);
}

TEST_CASE("summary compare rejects injected deviations")
{
    SUBCASE("frame off by two")
    {
        CHECK_FALSE(compare(patched(kSample, "10 cast", "12 cast")).passed());
    }
    SUBCASE("damage off by one")
    {
        CHECK_FALSE(compare(patched(kSample, "5001 120 0", "5001 121 0")).passed());
    }
    SUBCASE("crit flag differs")
    {
        CHECK_FALSE(compare(patched(kSample, "5001 120 0", "5001 120 1")).passed());
    }
    SUBCASE("missing line")
    {
        const auto report = compare(patched(kSample, "60 death role:9001#1\n", ""));
        CHECK_FALSE(report.passed());
        CHECK(report.differences.size() == 1);
        CHECK(report.differences[0].kind == Difference::Kind::Missing);
    }
    SUBCASE("extra line")
    {
        const auto report = compare(patched(kSample, "60 death", "59 death role:9001#1\n60 death"));
        CHECK_FALSE(report.passed());
        CHECK(report.differences[0].kind == Difference::Kind::Extra);
    }
    SUBCASE("displacement end point beyond tolerance")
    {
        CHECK_FALSE(compare(patched(kSample, "10.500 0.000 -2.250\n41", "13.600 0.000 -2.250\n41")).passed());
    }
    SUBCASE("final hp differs")
    {
        CHECK_FALSE(compare(patched(kSample, "final role:101#1 900", "final role:101#1 899")).passed());
    }
    SUBCASE("draw policy differs")
    {
        CHECK_FALSE(compare(patched(kSample, "crit=min", "crit=max")).passed());
    }
}

TEST_CASE("summary known deviation downgrades to warning")
{
    std::vector<KnownDeviation> deviations;
    std::string error;
    REQUIRE(parseKnownDeviations("# comment\nhp unit_* key=role:9001* Chill blocks healing\n", deviations, error));
    const std::string bad = patched(kSample, "21 hp role:9001#1 -8 dot", "21 hp role:9001#1 -9 dot");
    const auto report     = compare(bad, deviations);
    CHECK(report.passed());
    CHECK(report.warnings == 1);
    // 类型不匹配的偏差条目不生效。
    CHECK_FALSE(compare(patched(kSample, "5001 120 0", "5001 121 0"), deviations).passed());
}

TEST_CASE("summary report shows context and counts")
{
    const auto golden = parse(kSample);
    const auto actual = parse(patched(kSample, "5001 120 0", "5001 121 0"));
    const auto report = compareSummaries(golden, actual, {});
    std::ostringstream out;
    writeReport(report, golden, actual, out);
    const auto text = out.str();
    CHECK(text.find("first difference") != std::string::npos);
    CHECK(text.find(">> ") != std::string::npos);
    CHECK(text.find("errors=1") != std::string::npos);
}
