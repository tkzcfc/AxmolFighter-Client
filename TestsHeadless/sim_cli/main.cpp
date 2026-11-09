#include "SummaryExport.h"
#include "../summary/SummaryCompare.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace mugen;
using namespace mugen::sim;

// 工具版本号。
constexpr const char* kVersion = "mugen_sim_cli 0.1 (snapshot v2)";

// 场景清单条目（与 legacy_cli 的 ManifestEntry 一致）。
struct ManifestEntry
{
    std::string name;
    std::string script;
    int32_t room = 0;
    int32_t occupation = 1;
    int32_t level = 1;
    uint64_t seed = 31;
    uint64_t frames = 700;
};

// 打印用法。
void usage()
{
    std::fprintf(stderr,
                 "Usage:\n"
                 "  mugen_sim_cli --version\n"
                 "  mugen_sim_cli run-empty --frames N\n"
                 "  mugen_sim_cli run-summary --scenario <name> --crit min|max [--manifest path] [--output path]\n"
                 "  mugen_sim_cli compare-scenario --scenario <name> --crit min|max [--manifest path]\n"
                 "  mugen_sim_cli compare-scenario --all [--manifest path]\n"
                 "  mugen_sim_cli compare-summary <golden> <actual> [--deviations file]\n");
}

// 解析非负整数。
bool parseNumber(std::string_view text, uint64_t& value)
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

// 挂载 Content VFS 并加载配置。
void loadAssets()
{
    if (!test::ensureContentVfs())
        throw std::runtime_error("Cannot mount Content VFS");
    auto& config = conf::Config::instance();
    if (!config.loaded() && !config.load("/mugen/config/config.bin"))
        throw std::runtime_error("Cannot load config.bin");
    auto& avatar = *AvatarAssetCache::getInstance();
    if (!avatar.isLoaded() && !avatar.load("/mugen/config/avatar.bin"))
        throw std::runtime_error("Cannot load avatar.bin");
}

// 读取房间配置构建世界设置。
WorldSetup roomSetup(int32_t roomId, uint64_t seed)
{
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(roomId);
    WorldSetup setup;
    setup.seed   = seed;
    setup.mode   = WorldMode::Battle;
    setup.roomId = roomId;
    setup.mapKey = room ? room->mapKey : std::string{};
    return setup;
}

// 按职业装配离线技能并进入房间的战斗世界，供脚本驱动。
World combatWorld(int32_t roomId, uint64_t seed, int32_t occupation, int32_t level = 1)
{
    loadAssets();
    World world;
    auto setup = roomSetup(roomId, seed);
    PlayerSetup player;
    player.m_class  = static_cast<CharacterClass>(occupation);
    player.m_roleId = type_conversions::toRoleConfigId(player.m_class);
    player.m_level  = level;
    player.m_camp   = 2;
    player.m_slot   = 0;
    player.m_isAi   = false;
    setup.m_players.push_back(player);
    setup.m_stageId          = 0;
    setup.m_copyId           = 0;
    setup.m_aiEnabled        = false;
    setup.m_useScenarioHooks = false;
    if (bootstrapBattle(world, setup).empty())
        throw std::runtime_error("Cannot enter room " + std::to_string(roomId));
    return world;
}

// 读取脚本：frame slot event quadrant angle pressType。
std::map<uint64_t, FrameInput> loadScript(const std::string& path, uint64_t frames)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open command script: " + path);
    std::map<uint64_t, FrameInput> inputs;
    std::string line;
    uint64_t lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream row(line);
        uint64_t frame;
        uint32_t slot, event;
        int32_t quadrant = 0;
        PlayerCommand command;
        std::string extra;
        if (!(row >> frame >> slot >> event >> quadrant >> command.angle >> command.pressType) || (row >> extra) ||
            frame == 0 || frame > frames || event > 255)
            throw std::invalid_argument("Invalid command script line " + std::to_string(lineNumber));
        command.quadrant     = static_cast<ControlQuadrant>(quadrant);
        command.controlEvent = static_cast<ControlEvent>(event);
        auto& input          = inputs[frame];
        input.frame          = frame;
        input.players.push_back({slot, {command}});
    }
    return inputs;
}

// 读取场景清单：name script room class level seed frames。
std::vector<ManifestEntry> loadManifest(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open manifest: " + path);
    const auto slash      = path.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
    std::vector<ManifestEntry> entries;
    std::string line;
    int lineNo = 0;
    while (std::getline(file, line))
    {
        ++lineNo;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream row(line);
        ManifestEntry entry;
        std::string script;
        if (!(row >> entry.name) || entry.name[0] == '#')
            continue;
        if (!(row >> script >> entry.room >> entry.occupation >> entry.level >> entry.seed >> entry.frames))
            throw std::runtime_error("Invalid manifest line " + std::to_string(lineNo));
        entry.script = (!script.empty() && script[0] == '/') ? script : dir + script;
        entries.push_back(std::move(entry));
    }
    return entries;
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

// 按清单条目运行一次并写出摘要；outputPath 为空时打印到 stdout。
void runOne(const ManifestEntry& entry, const std::string& crit, const std::string& outputPath)
{
    const auto inputs = loadScript(entry.script, entry.frames);
    World world       = combatWorld(entry.room, entry.seed, entry.occupation, entry.level);
    sim_cli::ExportParams params;
    params.scenario = entry.name;
    params.room     = entry.room;
    params.seed     = entry.seed;
    params.frames   = entry.frames;
    params.crit     = crit;
    const auto result = sim_cli::exportSummary(world, inputs, params);
    if (outputPath.empty())
    {
        std::cout << summary::writeSummary(result);
        return;
    }
    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot open summary output: " + outputPath);
    out << summary::writeSummary(result);
}

// run-summary 命令。
int runSummaryCommand(int argc, char** argv)
{
    const char* usageText = "Expected run-summary --scenario <name> --crit min|max [--manifest path] [--output path]";
    std::string scenario, crit, output, manifest = "data/golden_summaries/scenarios.txt";
    for (int index = 2; index < argc; ++index)
    {
        const std::string_view arg(argv[index]);
        if (index + 1 >= argc)
            throw std::invalid_argument(usageText);
        const std::string value = argv[++index];
        if (arg == "--scenario")
            scenario = value;
        else if (arg == "--crit")
            crit = value;
        else if (arg == "--output")
            output = value;
        else if (arg == "--manifest")
            manifest = value;
        else
            throw std::invalid_argument(usageText);
    }
    if (scenario.empty() || (crit != "min" && crit != "max"))
        throw std::invalid_argument(usageText);
    for (const auto& entry : loadManifest(manifest))
        if (entry.name == scenario)
        {
            runOne(entry, crit, output);
            return 0;
        }
    throw std::invalid_argument("Unknown scenario in manifest: " + scenario);
}

// 读取文本文件全部内容。
std::string readText(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// 运行场景并与 golden 比对，返回 0（通过）或 1（失败）。
int compareOne(const ManifestEntry& entry, const std::string& crit, const std::string& goldenDir,
               const std::vector<summary::KnownDeviation>& deviations, std::ostream& out)
{
    summary::Summary golden;
    std::string parseError;
    const std::string goldenPath = goldenDir + "/" + entry.name + "." + crit + ".summary";
    if (!summary::parseSummary(readText(goldenPath), golden, parseError))
        throw std::runtime_error("Cannot parse golden: " + parseError);

    const auto inputs = loadScript(entry.script, entry.frames);
    World world       = combatWorld(entry.room, entry.seed, entry.occupation, entry.level);
    sim_cli::ExportParams params;
    params.scenario = entry.name;
    params.room     = entry.room;
    params.seed     = entry.seed;
    params.frames   = entry.frames;
    params.crit     = crit;
    const auto actual = sim_cli::exportSummary(world, inputs, params);

    const auto report = summary::compareSummaries(golden, actual, deviations);
    out << (report.passed() ? "PASS" : "FAIL") << ' ' << entry.name << ' ' << crit;
    if (!report.passed())
        out << " errors=" << report.errors;
    out << " (golden=" << report.goldenLines << " actual=" << report.actualLines << " aligned=" << report.aligned << ")\n";
    if (!report.passed())
        summary::writeReport(report, golden, actual, out);
    return report.passed() ? 0 : 1;
}

// compare-scenario 命令：--scenario/--all。
int compareScenarioCommand(int argc, char** argv)
{
    const char* usageText =
        "Expected compare-scenario (--scenario <name> --crit min|max | --all) [--manifest path]";
    std::string scenario, crit, manifest = "data/golden_summaries/scenarios.txt";
    bool all = false;
    for (int index = 2; index < argc; ++index)
    {
        const std::string_view arg(argv[index]);
        if (arg == "--all")
        {
            all = true;
            continue;
        }
        if (index + 1 >= argc)
            throw std::invalid_argument(usageText);
        const std::string value = argv[++index];
        if (arg == "--scenario")
            scenario = value;
        else if (arg == "--crit")
            crit = value;
        else if (arg == "--manifest")
            manifest = value;
        else
            throw std::invalid_argument(usageText);
    }

    std::vector<summary::KnownDeviation> deviations;
    const auto slash = manifest.find_last_of("/\\");
    const std::string goldenDir = slash == std::string::npos ? std::string(".") : manifest.substr(0, slash);
    const std::string deviationsPath = goldenDir + "/known_deviations.txt";
    {
        std::ifstream file(deviationsPath);
        if (file)
        {
            std::string error;
            std::ostringstream buffer;
            buffer << file.rdbuf();
            if (!summary::parseKnownDeviations(buffer.str(), deviations, error))
                throw std::runtime_error("Cannot parse deviations: " + error);
        }
    }

    int rc = 0;
    if (all)
    {
        for (const auto& entry : loadManifest(manifest))
            for (const char* mode : {"min", "max"})
                rc |= compareOne(entry, mode, goldenDir, deviations, std::cout);
        return rc;
    }
    if (scenario.empty() || (crit != "min" && crit != "max"))
        throw std::invalid_argument(usageText);
    for (const auto& entry : loadManifest(manifest))
        if (entry.name == scenario)
            return compareOne(entry, crit, goldenDir, deviations, std::cout);
    throw std::invalid_argument("Unknown scenario in manifest: " + scenario);
}
}  // namespace

// 命令分发入口。
int main(int argc, char** argv)
{
    try
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
        if (command == "run-summary")
            return runSummaryCommand(argc, argv);
        if (command == "compare-scenario")
            return compareScenarioCommand(argc, argv);
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
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
