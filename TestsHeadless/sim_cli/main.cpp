#include "mugen/sim/World.h"
#include "../summary/SummaryCompare.h"

#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
using namespace mugen;
using namespace mugen::sim;

// 工具版本号。
constexpr const char* kVersion = "mugen_sim_cli 0.1 (snapshot v2)";

// 打印用法。
void usage()
{
    std::fprintf(stderr,
                 "Usage:\n"
                 "  mugen_sim_cli --version\n"
                 "  mugen_sim_cli run-empty --frames N\n"
                 "  mugen_sim_cli compare-summary <golden> <actual> [--deviations file]\n");
}

// 解析非负整数。
bool parseNumber(std::string_view text, uint64_t& value)
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

// run-empty：空世界推进 N 帧并输出最终哈希。
int runEmpty(int argc, char** argv)
{
    uint64_t frames = 0;
    bool haveFrames = false;
    for (int i = 2; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--frames") == 0)
            haveFrames = parseNumber(argv[i + 1], frames);
    if (!haveFrames)
    {
        usage();
        return 2;
    }
    World world;
    WorldSetup setup;
    world.init(setup);
    for (uint64_t i = 0; i < frames; ++i)
    {
        FrameInput input;
        input.frame = world.frame() + 1;
        world.tick(input);
    }
    std::printf("frames=%" PRIu64 " hash=%016" PRIx64 "\n", frames, world.hash());
    return 0;
}
}  // namespace

// 命令分发入口。
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage();
        return 2;
    }
    const std::string_view command = argv[1];
    if (command == "--version")
    {
        std::printf("%s\n", kVersion);
        return 0;
    }
    if (command == "run-empty")
        return runEmpty(argc, argv);
    // compare-summary：S0 只比对两个摘要文件；S1 起改为用新 sim 跑场景后与 golden 比对。
    if (command == "compare-summary")
    {
        std::string deviations;
        if (argc == 6 && std::string_view(argv[4]) == "--deviations")
            deviations = argv[5];
        else if (argc != 4)
        {
            usage();
            return 2;
        }
        return summary::runCompareSummary(argv[2], argv[3], deviations, std::cout);
    }
    usage();
    return 2;
}
