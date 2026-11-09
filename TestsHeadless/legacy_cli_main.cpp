#include "support/ScenarioHarness.h"
#include "legacy_cli/SummaryExport.h"
#include "legacy_cli/SummaryManifest.h"
#include "summary/SummaryCompare.h"
#include "support/TraceDiff.h"
#include "mugen/conf/ConfigFormat.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/sim_legacy/BattleBootstrap.h"
#include "mugen/sim_legacy/Replay.h"
#include "mugen/sim_legacy/rollback/RollbackSession.h"
#include "mugen/sim_legacy/room/RoomDirector.h"
#include "mugen/sim_legacy/spawn/RoleSpawn.h"
#include "mugen/sim_legacy/trace/TraceSink.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{
using namespace mugen;
using namespace mugen::sim_legacy;
bool aiEnabledFlag(std::string_view text)
{
    if (text == "on")
        return true;
    if (text == "off")
        return false;
    throw std::invalid_argument("Expected --ai on|off");
}
uint64_t number(std::string_view text)
{
    uint64_t value          = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw std::invalid_argument("Expected an unsigned integer: " + std::string(text));
    return value;
}
void printHash(const World& world)
{
    std::cout << "world hash " << world.ecs().singleton<WorldClock>().frame << ' ' << std::hex << world.hash()
              << std::dec << '\n';
}
std::map<uint64_t, uint64_t> loadHashes(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open hash file: " + path);
    std::map<uint64_t, uint64_t> hashes;
    std::string line;
    uint64_t lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream row(line);
        uint64_t frame = 0;
        std::string hash;
        std::string extra;
        if (!(row >> frame >> hash) || (row >> extra) || frame == 0)
            throw std::invalid_argument("Invalid hash file line " + std::to_string(lineNumber));
        uint64_t value          = 0;
        const auto [end, error] = std::from_chars(hash.data(), hash.data() + hash.size(), value, 16);
        if (error != std::errc{} || end != hash.data() + hash.size() || !hashes.emplace(frame, value).second)
            throw std::invalid_argument("Invalid hash file line " + std::to_string(lineNumber));
    }
    if (!file.eof())
        throw std::runtime_error("Cannot read hash file: " + path);
    return hashes;
}
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
    if (!file.eof())
        throw std::runtime_error("Cannot read command script: " + path);
    return inputs;
}
World recordedWorld(const WorldSetup& setup)
{
    if (setup.mode == WorldMode::Battle)
    {
        test::ScenarioHarness::loadAssets();
        World world(setup.m_useScenarioHooks ? test::ScenarioHarness::roomHooks() : WorldHooks{});
        if (bootstrapBattle(world, setup).empty())
            throw std::runtime_error("Cannot bootstrap replay world");
        return world;
    }
    World world;
    world.init(setup);
    return world;
}
FrameInput recordedInput(World& world)
{
    FrameInput input;
    input.frame      = world.ecs().singleton<WorldClock>().frame + 1;
    const auto& room = world.ecs().singleton<mugen::sim_legacy::RoomState>();
    if (room.m_state == mugen::RoomState::ReadySpine)
        input.systemCommands.push_back(RoomWarningDone{room.roomId});
    PlayerCommand command;
    command.controlEvent = room.m_victory ? ControlEvent::CTR_RUN
                                          : (input.frame % 30 == 0 ? ControlEvent::CTR_ATKA : ControlEvent::CTR_WALK);
    command.quadrant     = ControlQuadrant::UpRight;
    command.angle        = 0;
    command.pressType    = 1;
    input.players.push_back({0, {command}});
    return input;
}
// 构建一个按职业装配离线技能并进入房间的战斗世界，供脚本驱动。
World combatWorld(int32_t roomId, uint64_t seed, int32_t occupation, int32_t level = 1)
{
    test::ScenarioHarness::loadAssets();
    World world;  // 默认 WorldHooks：Role 组走真实 role::update 战斗逻辑
    auto setup               = test::ScenarioHarness::roomSetup(roomId, seed);
    PlayerSetup player;
    player.m_class           = static_cast<CharacterClass>(occupation);
    player.m_roleId          = type_conversions::toRoleConfigId(player.m_class);
    player.m_level           = level;
    player.m_camp            = 2;
    player.m_slot            = 0;
    player.m_isAi            = false;
    setup.m_players.push_back(player);
    setup.m_stageId          = 0;
    setup.m_copyId           = 0;
    setup.m_useScenarioHooks = false;
    if (bootstrapBattle(world, setup).empty())
        throw std::runtime_error("Cannot enter room " + std::to_string(roomId));
    if (std::getenv("MUGEN_TRACE_DUMP_SKILLS"))
    {
        world.ecs().each<RoleIdentity, SkillPool, Vitals>(
            [&](Entity, const RoleIdentity& id, const SkillPool& pool, const Vitals& vitals) {
            std::cerr << "[dump] role " << id.roleId << " ep=" << vitals.ep.toInt() << "/" << vitals.maxEp.toInt()
                      << " crystals=" << pool.m_crystals << '\n';
            for (size_t s = 0; s < pool.m_slots.size(); ++s)
            {
                std::cerr << "  slot '" << pool.m_slots[s].m_name << "':";
                for (const auto& idx : pool.m_index)
                    if (idx.m_address.m_slot == s)
                    {
                        const auto& sk  = pool.m_skills[idx.m_skill];
                        const auto* row = conf::Config::instance().find<conf::SkillAttackConfig>(sk.m_configId);
                        std::cerr << " [" << idx.m_address.m_index << "." << idx.m_address.m_mode << "."
                                  << idx.m_address.m_step << " cfg=" << sk.m_configId << " mp=" << (row ? row->mp : -1)
                                  << " ep=" << (row ? row->ep : -1) << " crystal=" << (row ? row->crystal : -1)
                                  << " rage=" << (row ? row->rage : -1) << " type=" << (row ? row->type : -1)
                                  << " press=" << (row ? row->pressTime : -1) << "]";
                    }
                std::cerr << '\n';
            }
        });
    }
    return world;
}

// 导出一份场景摘要：按清单条目装配世界、读脚本并写文件。
void exportOne(const legacy_cli::ManifestEntry& entry, const std::string& crit, const std::string& outputPath)
{
    const auto inputs = loadScript(entry.script, entry.frames);
    World world       = combatWorld(entry.room, entry.seed, entry.occupation, entry.level);
    legacy_cli::ExportParams params;
    params.scenario = entry.name;
    params.room     = entry.room;
    params.seed     = entry.seed;
    params.frames   = entry.frames;
    params.crit     = crit;
    const auto result = legacy_cli::exportSummary(world, inputs, params);
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
// export-summary 命令：--scenario/--all 选择清单条目，--crit min|max（--all 时两种都导出）。
int exportSummaryCommand(int argc, char** argv)
{
    const char* usage =
        "Expected export-summary (--scenario <name> --crit min|max [--output path] | --all [--out-dir dir]) "
        "[--manifest path]";
    std::string scenario, crit, output, manifest = "data/golden_summaries/scenarios.txt", outDir;
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
            throw std::invalid_argument(usage);
        const std::string value = argv[++index];
        if (arg == "--scenario")
            scenario = value;
        else if (arg == "--crit")
            crit = value;
        else if (arg == "--output")
            output = value;
        else if (arg == "--manifest")
            manifest = value;
        else if (arg == "--out-dir")
            outDir = value;
        else
            throw std::invalid_argument(usage);
    }
    const auto entries = legacy_cli::loadManifest(manifest);
    if (all)
    {
        if (outDir.empty())
        {
            const auto slash = manifest.find_last_of("/\\");
            outDir           = slash == std::string::npos ? std::string(".") : manifest.substr(0, slash);
        }
        for (const auto& entry : entries)
            for (const char* mode : {"min", "max"})
                exportOne(entry, mode, outDir + "/" + entry.name + "." + mode + ".summary");
        return 0;
    }
    if (scenario.empty() || (crit != "min" && crit != "max"))
        throw std::invalid_argument(usage);
    for (const auto& entry : entries)
        if (entry.name == scenario)
        {
            exportOne(entry, crit, output);
            return 0;
        }
    throw std::invalid_argument("Unknown scenario in manifest: " + scenario);
}

// ===== 回滚成本基准 =====
using BenchClock = std::chrono::steady_clock;
double benchUsSince(BenchClock::time_point start)
{
    return std::chrono::duration<double, std::micro>(BenchClock::now() - start).count();
}
struct BenchStats
{
    double avg = 0;
    double p50 = 0;
    double p99 = 0;
};
BenchStats benchSummarize(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const auto pct   = [&](double p) {
        const size_t index = static_cast<size_t>(p * static_cast<double>(samples.size() - 1) + 0.5);
        return samples[std::min(index, samples.size() - 1)];
    };
    return {sum / static_cast<double>(samples.size()), pct(0.50), pct(0.99)};
}
void benchPrintStat(std::string_view name, const BenchStats& stats)
{
    std::cout << name << ": avg=" << stats.avg << "us p50=" << stats.p50 << "us p99=" << stats.p99 << "us\n";
}
// 生成确定性一帧输入：周期攻击，其余时间朝对方移动，驱动全部玩家槽位交战。
FrameInput benchInput(uint64_t frame, int playerCount)
{
    FrameInput input;
    input.frame = frame;
    for (int slot = 0; slot < playerCount; ++slot)
    {
        PlayerCommand command;
        if (frame % 24 == 0)
            command.controlEvent = ControlEvent::CTR_ATKA;
        else if (frame % 8 < 5)
            command.controlEvent = ControlEvent::CTR_RUN;
        else
            command.controlEvent = ControlEvent::CTR_WALK;
        command.quadrant  = (slot % 2 == 0) ? ControlQuadrant::UpRight : ControlQuadrant::DownLeft;
        command.angle     = 0;
        command.pressType = 1;
        input.players.push_back({static_cast<uint32_t>(slot), {command}});
    }
    return input;
}
// 建立含 playerCount 个互相敌对玩家并进入房间的战斗世界。
World benchWorld(int32_t roomId, uint64_t seed, int playerCount)
{
    test::ScenarioHarness::loadAssets();
    World world;  // 默认 WorldHooks：Role 组走真实 role::update 战斗逻辑
    world.init(test::ScenarioHarness::roomSetup(roomId, seed));
    for (int slot = 0; slot < playerCount; ++slot)
    {
        RoleSpawnParams params;
        params.camp       = slot == 0 ? 2 : 8;  // 槽位 0 与其余槽位互为敌对
        params.level      = 100;                 // 与怪物等级接近，避免热身阶段清场
        params.isAi       = false;  // 由脚本输入驱动
        params.playerSlot = static_cast<uint32_t>(slot);
        if (RoleSpawner(world).spawnPlayer(CharacterClass::kSwordman, std::move(params)).value == 0)
            throw std::runtime_error("Cannot spawn player slot " + std::to_string(slot));
    }
    const auto* config = conf::Config::instance().find<conf::RoomConfig>(roomId);
    if (!config || !room::RoomDirector(world).enter(*config, 0, 0))
        throw std::runtime_error("Cannot enter room " + std::to_string(roomId));
    return world;
}
// 逐帧推进进入战斗，期间确认房间警告以越过 ReadySpine。
void benchWarmup(World& world, uint64_t frames, int playerCount)
{
    for (uint64_t frame = 1; frame <= frames; ++frame)
    {
        auto input       = benchInput(frame, playerCount);
        const auto& room = world.ecs().singleton<mugen::sim_legacy::RoomState>();
        if (room.m_state == mugen::RoomState::ReadySpine)
            input.systemCommands.push_back(RoomWarningDone{room.roomId});
        world.tick(input);
    }
}
// 统计具有指定组件的实体数量。
template <class Component>
size_t benchCount(World& world)
{
    size_t count = 0;
    world.ecs().each<Component>([&](Entity, Component&) { ++count; });
    return count;
}
// 输出测量时的实体与组件池规模。
void benchPrintEntities(World& world)
{
    size_t total = 0;
    world.ecs().each<>([&](Entity) { ++total; });
    std::array<size_t, 5> groups{};
    world.ecs().each<EntityGroup>([&](Entity, const EntityGroup& group) {
        const auto index = static_cast<size_t>(group.kind);
        if (index < groups.size())
            ++groups[index];
    });
    std::cout << "entities: total=" << total << " role=" << groups[0] << " effect=" << groups[1]
              << " obstacle=" << groups[2] << " goods=" << groups[3] << " portal=" << groups[4] << '\n';
    std::cout << "components: RoleIdentity=" << benchCount<RoleIdentity>(world)
              << " SkillPool=" << benchCount<SkillPool>(world) << " BuffPool=" << benchCount<BuffPool>(world)
              << " AiAgent=" << benchCount<AiAgent>(world) << " ColliderState=" << benchCount<ColliderState>(world)
              << " EffectState=" << benchCount<EffectState>(world) << " MotionState=" << benchCount<MotionState>(world)
              << '\n';
}
// 用模拟延迟驱动联机回滚会话，校验最终确认帧哈希并打印统计。
int rollbackSim(int32_t roomId, int playerCount, uint64_t frames, uint64_t lag, uint64_t seed)
{
    World reference = benchWorld(roomId, seed, playerCount);

    std::vector<FrameInput> script;
    script.reserve(frames);
    for (uint64_t f = 1; f <= frames; ++f)
        script.push_back(benchInput(f, playerCount));

    std::vector<uint64_t> hashes;
    hashes.reserve(frames);
    for (const auto& input : script)
    {
        reference.tick(input);
        hashes.push_back(reference.hash());
    }

    rollback::RollbackSession session(0, static_cast<uint32_t>(playerCount));
    session.setWorld(benchWorld(roomId, seed, playerCount));

    for (uint64_t frame = 1; frame <= frames; ++frame)
    {
        const auto& frameInput = script[frame - 1];
        for (const auto& player : frameInput.players)
            if (player.slot == 0)
                session.addLocalInput(frame, player);
        for (int slot = 0; slot < playerCount; ++slot)
        {
            const uint64_t confirmed = frame >= lag + static_cast<uint64_t>(slot)
                                           ? frame - lag - static_cast<uint64_t>(slot)
                                           : 0;
            if (confirmed >= 1)
                for (const auto& player : script[confirmed - 1].players)
                    if (player.slot == static_cast<uint32_t>(slot))
                        session.addConfirmedInput(confirmed, static_cast<uint32_t>(slot), player);
        }
        if (!session.advanceTo(frame))
        {
            std::cout << "rollback-sim stalled at frame " << frame << '\n';
            break;
        }
    }

    const uint64_t confirmed = session.confirmedFrame();
    const bool match         = confirmed > 0 && session.hashAt(confirmed) == hashes[confirmed - 1];
    std::cout << "rollback-sim room=" << roomId << " players=" << playerCount << " frames=" << frames
              << " lag=" << lag << " seed=" << seed << '\n';
    std::cout << "confirmed frame " << confirmed << " hash " << std::hex << session.hashAt(confirmed) << std::dec
              << (match ? " MATCH" : " MISMATCH") << '\n';
    std::cout << "stats rollbacks=" << session.stats().rollbacks << " maxDepth=" << session.stats().maxDepth
              << " resimFrames=" << session.stats().resimFrames << " stalls=" << session.stats().stalls
              << " mispredictions=" << session.stats().mispredictions << '\n';
    return match ? 0 : 1;
}
}  // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--version")
        {
            std::cout << "mugen_legacy_cli 0.1.0\n";
            return 0;
        }
        if (argc >= 4 && std::string_view(argv[1]) == "compare-summary")
        {
            // compare-summary <golden> <actual> [--deviations file]
            std::string deviations;
            if (argc == 6 && std::string_view(argv[4]) == "--deviations")
                deviations = argv[5];
            else if (argc != 4)
                throw std::invalid_argument("Expected compare-summary <golden> <actual> [--deviations file]");
            return summary::runCompareSummary(argv[2], argv[3], deviations, std::cout);
        }
        if (argc >= 2 && std::string_view(argv[1]) == "export-summary")
            return exportSummaryCommand(argc, argv);
        if (argc >= 3 && std::string_view(argv[1]) == "replay")
        {
            uint64_t every = 1;
            std::string compare;
            std::string tracePath;
            std::string revision;
            std::optional<bool> aiEnabledOverride;
            for (int index = 3; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument(
                        "Expected replay <file> [--hash-every N] [--compare hashfile] [--trace file] [--revision rev] "
                        "[--ai on|off]");
                if (std::string_view(argv[index]) == "--hash-every")
                    every = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--compare")
                    compare = argv[index + 1];
                else if (std::string_view(argv[index]) == "--trace")
                    tracePath = argv[index + 1];
                else if (std::string_view(argv[index]) == "--revision")
                    revision = argv[index + 1];
                else if (std::string_view(argv[index]) == "--ai")
                    aiEnabledOverride = aiEnabledFlag(argv[index + 1]);
                else
                    throw std::invalid_argument(
                        "Expected replay <file> [--hash-every N] [--compare hashfile] [--trace file] [--revision rev] "
                        "[--ai on|off]");
            }
            if (every == 0)
                throw std::invalid_argument("Hash interval must be positive");
            const auto expected = compare.empty() ? std::map<uint64_t, uint64_t>{} : loadHashes(compare);
            test::ScenarioHarness::loadAssets();
            Replay replay;
            std::string error;
            if (!replay.load(argv[2], conf::configSchemaHash(), error))
                throw std::runtime_error(error);
            World world = recordedWorld(replay.setup);
            if (aiEnabledOverride.has_value())
                world.setAiEnabled(*aiEnabledOverride);
            trace::TraceSink sink;
            if (!tracePath.empty())
            {
                world.setTrace(&sink);
                sink.write("# mugen_sim_cli trace v1");
                if (!revision.empty())
                    sink.write("# revision " + revision);
                sink.write("# room " + std::to_string(replay.setup.roomId) + " seed " +
                           std::to_string(replay.setup.seed));
            }
            for (const auto& input : replay.frames)
            {
                world.tick(input);
                const auto expectedHash = expected.find(input.frame);
                if (expectedHash != expected.end() && expectedHash->second != world.hash())
                    throw std::runtime_error("Hash mismatch at frame " + std::to_string(input.frame));
                if (input.frame % every == 0)
                    printHash(world);
            }
            for (const auto& [frame, hash] : expected)
                if (frame > replay.frames.size())
                    throw std::runtime_error("Hash file frame is outside replay: " + std::to_string(frame));
            if (replay.frames.empty() || replay.frames.back().frame % every != 0)
                printHash(world);
            if (!tracePath.empty())
            {
                std::ofstream out(tracePath);
                if (!out)
                    throw std::runtime_error("Cannot open trace file: " + tracePath);
                out << sink.text();
            }
            return 0;
        }
        if (argc >= 6 && std::string_view(argv[1]) == "record-room" && std::string_view(argv[3]) == "--frames")
        {
            const auto roomId = number(argv[2]);
            const auto frames = number(argv[4]);
            if (roomId > INT32_MAX || frames == 0 || std::string_view(argv[5]).empty())
                throw std::invalid_argument("Expected record-room <roomId> --frames N <file> [--ai on|off]");
            bool aiEnabled = true;
            for (int index = 6; index < argc; index += 2)
            {
                if (index + 1 >= argc || std::string_view(argv[index]) != "--ai")
                    throw std::invalid_argument("Expected record-room <roomId> --frames N <file> [--ai on|off]");
                aiEnabled = aiEnabledFlag(argv[index + 1]);
            }
            Replay replay;
            replay.schemaHash = conf::configSchemaHash();
            replay.setup      = test::ScenarioHarness::roomSetup(static_cast<int32_t>(roomId), 31);
            PlayerSetup recordPlayer;
            recordPlayer.m_roleId = 0;
            recordPlayer.m_camp   = 2;
            recordPlayer.m_slot   = 0;
            replay.setup.m_players.push_back(recordPlayer);
            replay.setup.m_stageId          = 0;
            replay.setup.m_copyId           = 0;
            replay.setup.m_aiEnabled        = aiEnabled;
            replay.setup.m_useScenarioHooks = true;
            World world       = recordedWorld(replay.setup);
            for (uint64_t frame = 1; frame <= frames; ++frame)
            {
                auto input = recordedInput(world);
                replay.frames.push_back(input);
                world.tick(input);
            }
            std::string error;
            if (!replay.save(argv[5], error))
                throw std::runtime_error(error);
            printHash(world);
            return 0;
        }
        if (argc >= 5 && std::string_view(argv[1]) == "run-room")
        {
            const auto room = number(argv[2]);
            if (room > INT32_MAX || std::string_view(argv[3]) != "--frames")
                throw std::invalid_argument(
                    "Expected run-room <roomId> --frames N [--class 1-7] [--seed N] [--script file] [--trace file] "
                    "[--revision rev] [--ai on|off]");
            const auto frames = number(argv[4]);
            std::map<uint64_t, FrameInput> inputs;
            std::string tracePath;
            std::string revision;
            int32_t occupation = 0;
            uint64_t seed      = 31;
            int32_t level      = 1;
            bool aiEnabled     = true;
            for (int index = 5; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument(
                        "Expected run-room <roomId> --frames N [--class 1-7] [--seed N] [--level N] [--script file] "
                        "[--trace file] [--revision rev] [--ai on|off]");
                if (std::string_view(argv[index]) == "--script")
                    inputs = loadScript(argv[index + 1], frames);
                else if (std::string_view(argv[index]) == "--trace")
                    tracePath = argv[index + 1];
                else if (std::string_view(argv[index]) == "--revision")
                    revision = argv[index + 1];
                else if (std::string_view(argv[index]) == "--class")
                    occupation = static_cast<int32_t>(number(argv[index + 1]));
                else if (std::string_view(argv[index]) == "--seed")
                    seed = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--level")
                    level = static_cast<int32_t>(number(argv[index + 1]));
                else if (std::string_view(argv[index]) == "--ai")
                    aiEnabled = aiEnabledFlag(argv[index + 1]);
                else
                    throw std::invalid_argument(
                        "Expected run-room <roomId> --frames N [--class 1-7] [--seed N] [--level N] [--script file] "
                        "[--trace file] [--revision rev] [--ai on|off]");
            }
            World world;
            if (occupation >= 1 && occupation <= 7)
                world = combatWorld(static_cast<int32_t>(room), seed, occupation, level);
            else
                world.init(test::ScenarioHarness::roomSetup(static_cast<int32_t>(room), seed));
            world.setAiEnabled(aiEnabled);
            trace::TraceSink sink;
            if (!tracePath.empty())
            {
                world.setTrace(&sink);
                sink.write("# mugen_sim_cli trace v1");
                if (!revision.empty())
                    sink.write("# revision " + revision);
                sink.write("# room " + std::to_string(world.setup().roomId) + " seed " +
                           std::to_string(world.setup().seed));
            }
            for (uint64_t frame = 1; frame <= frames; ++frame)
            {
                auto input = inputs.extract(frame);
                world.tick(input.empty() ? FrameInput{frame, {}, {}, {}} : input.mapped());
                if (std::getenv("MUGEN_DUMP_FRAME"))
                    world.ecs().each<RoleIdentity, RoleStatus, SkillPool, EntityGroup, ControlEventQueue, Vitals>(
                        [&](Entity, const RoleIdentity& id, const RoleStatus& st, const SkillPool& pool,
                            const EntityGroup& g, const ControlEventQueue& q, const Vitals& vitals) {
                        if (id.roleId == 101 || id.roleId == 102 || id.roleId == 104)
                            std::cerr << "[f" << frame << "] role=" << id.roleId
                                      << " st=" << static_cast<uint32_t>(st.eEntityStatus) << " cur=" << pool.m_current
                                      << " ep=" << vitals.ep.toInt() << " mp=" << vitals.mp.toInt()
                                      << " susp=" << g.suspended << '\n';
                    });
            }
            printHash(world);
            if (!tracePath.empty())
            {
                std::ofstream out(tracePath);
                if (!out)
                    throw std::runtime_error("Cannot open trace file: " + tracePath);
                out << sink.text();
            }
            return 0;
        }
        if (argc >= 4 && std::string_view(argv[1]) == "trace-diff")
        {
            std::string mode = "full";
            std::string allowPath;
            std::vector<std::string> files;
            for (int index = 2; index < argc; ++index)
            {
                const std::string_view arg(argv[index]);
                if (arg == "--mode" || arg == "--allow")
                {
                    if (index + 1 >= argc)
                        throw std::invalid_argument(
                            "Expected trace-diff [--mode full|effects] [--allow file] <a.trace> <b.trace>");
                    if (arg == "--mode")
                        mode = argv[++index];
                    else
                        allowPath = argv[++index];
                }
                else
                    files.emplace_back(argv[index]);
            }
            if (files.size() != 2 || (mode != "full" && mode != "effects"))
                throw std::invalid_argument(
                    "Expected trace-diff [--mode full|effects] [--allow file] <a.trace> <b.trace>");
            if (mode == "full" && !allowPath.empty())
                throw std::invalid_argument("Allow list requires --mode effects");
            const auto readText = [](const std::string& path) {
                std::ifstream file(path);
                if (!file)
                    throw std::runtime_error("Cannot open trace file: " + path);
                std::ostringstream buffer;
                buffer << file.rdbuf();
                return buffer.str();
            };
            if (mode == "effects")
            {
                std::vector<std::string> patterns;
                if (!allowPath.empty())
                {
                    std::ifstream file(allowPath);
                    if (!file)
                        throw std::runtime_error("Cannot open allow file: " + allowPath);
                    std::string line;
                    while (std::getline(file, line))
                    {
                        if (!line.empty() && line.back() == '\r')
                            line.pop_back();
                        if (line.empty() || line.front() == '#')
                            continue;
                        patterns.push_back(line);
                    }
                }
                const auto diff = effects_trace::diffEffects(readText(files[0]), readText(files[1]), patterns);
                std::cout << diff.report;
                return diff.matched ? 0 : 1;
            }
            std::ifstream fa(files[0]);
            std::ifstream fb(files[1]);
            if (!fa)
                throw std::runtime_error("Cannot open trace file: " + files[0]);
            if (!fb)
                throw std::runtime_error("Cannot open trace file: " + files[1]);
            std::string la, lb;
            uint64_t lineNumber = 0;
            int printed         = 0;
            while (true)
            {
                const bool gotA = static_cast<bool>(std::getline(fa, la));
                const bool gotB = static_cast<bool>(std::getline(fb, lb));
                if (!gotA && !gotB)
                    break;
                ++lineNumber;
                if (gotA != gotB || la != lb)
                {
                    if (printed < 20)
                    {
                        std::cout << "line " << lineNumber << ":\n";
                        std::cout << "  a: " << (gotA ? la : std::string("<eof>")) << '\n';
                        std::cout << "  b: " << (gotB ? lb : std::string("<eof>")) << '\n';
                    }
                    ++printed;
                }
            }
            return printed == 0 ? 0 : 1;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "bench-rollback")
        {
            int32_t roomId  = 2010101;
            int playerCount = 2;
            uint64_t samples = 200;
            uint64_t warmup  = 300;
            for (int index = 2; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument(
                        "Expected bench-rollback [--room N] [--players N] [--samples N] [--warmup N]");
                if (std::string_view(argv[index]) == "--room")
                    roomId = static_cast<int32_t>(number(argv[index + 1]));
                else if (std::string_view(argv[index]) == "--players")
                    playerCount = static_cast<int>(number(argv[index + 1]));
                else if (std::string_view(argv[index]) == "--samples")
                    samples = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--warmup")
                    warmup = number(argv[index + 1]);
                else
                    throw std::invalid_argument(
                        "Expected bench-rollback [--room N] [--players N] [--samples N] [--warmup N]");
            }
            if (playerCount < 1 || playerCount > kMaxBattlePlayers)
                throw std::invalid_argument("--players must be between 1 and " + std::to_string(kMaxBattlePlayers));
            if (samples == 0 || warmup == 0)
                throw std::invalid_argument("--samples and --warmup must be positive");

            World world = benchWorld(roomId, 31, playerCount);
            benchWarmup(world, warmup, playerCount);

            std::cout << "bench-rollback room=" << roomId << " players=" << playerCount << " samples=" << samples
                      << " warmup=" << warmup << " frame=" << world.ecs().singleton<WorldClock>().frame << '\n';
            benchPrintEntities(world);

            // 冻结在战斗状态，重复采样快照/克隆/哈希。
            std::vector<double> cloneUs, serUs, deserUs, hashUs;
            cloneUs.reserve(samples);
            serUs.reserve(samples);
            deserUs.reserve(samples);
            hashUs.reserve(samples);
            size_t snapshotBytes = 0;
            for (uint64_t sample = 0; sample < samples; ++sample)
            {
                {
                    const auto start = BenchClock::now();
                    World copy       = world.clone();
                    cloneUs.push_back(benchUsSince(start));
                    (void)copy;
                }
                {
                    ByteBuffer buffer;
                    const auto start = BenchClock::now();
                    world.serialize(buffer);
                    serUs.push_back(benchUsSince(start));
                    snapshotBytes = buffer.len();
                    World restored;
                    const auto startDeserialize = BenchClock::now();
                    const bool ok               = restored.deserialize(buffer);
                    deserUs.push_back(benchUsSince(startDeserialize));
                    if (!ok)
                        throw std::runtime_error("Snapshot deserialize failed during bench");
                }
                {
                    const auto start = BenchClock::now();
                    const uint64_t h = world.hash();
                    hashUs.push_back(benchUsSince(start));
                    (void)h;
                }
            }

            // 完整回滚周期：从快照恢复并重推 N 帧。
            constexpr std::array<uint64_t, 3> rollbackN{4, 8, 12};
            std::array<std::vector<double>, rollbackN.size()> rollbackUs;
            for (size_t ni = 0; ni < rollbackN.size(); ++ni)
                rollbackUs[ni].reserve(samples);
            const uint64_t baseFrame = world.ecs().singleton<WorldClock>().frame;
            for (size_t ni = 0; ni < rollbackN.size(); ++ni)
            {
                const uint64_t n = rollbackN[ni];
                std::vector<FrameInput> inputs;
                inputs.reserve(n);
                for (uint64_t k = 1; k <= n; ++k)
                    inputs.push_back(benchInput(baseFrame + k, playerCount));
                for (uint64_t sample = 0; sample < samples; ++sample)
                {
                    ByteBuffer snapshot;
                    world.serialize(snapshot);
                    World restored;
                    const auto start = BenchClock::now();
                    if (!restored.deserialize(snapshot))
                        throw std::runtime_error("Rollback snapshot deserialize failed");
                    for (const auto& input : inputs)
                        restored.tick(input);
                    rollbackUs[ni].push_back(benchUsSince(start));
                }
            }

            // 在真实推进状态上采样 tick。
            std::vector<double> tickUs;
            tickUs.reserve(samples);
            const uint64_t tickFrame = world.ecs().singleton<WorldClock>().frame;
            for (uint64_t sample = 0; sample < samples; ++sample)
            {
                const auto input = benchInput(tickFrame + sample + 1, playerCount);
                const auto start = BenchClock::now();
                world.tick(input);
                tickUs.push_back(benchUsSince(start));
            }

            std::cout << "snapshot bytes: " << snapshotBytes << '\n';
            benchPrintStat("tick", benchSummarize(tickUs));
            benchPrintStat("clone", benchSummarize(cloneUs));
            benchPrintStat("serialize", benchSummarize(serUs));
            benchPrintStat("deserialize", benchSummarize(deserUs));
            benchPrintStat("hash", benchSummarize(hashUs));
            for (size_t ni = 0; ni < rollbackN.size(); ++ni)
                benchPrintStat("rollback N=" + std::to_string(rollbackN[ni]), benchSummarize(rollbackUs[ni]));
            return 0;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "rollback-sim")
        {
            int32_t roomId     = 30002;
            int playerCount    = 2;
            uint64_t frames    = 300;
            uint64_t lag       = 4;
            uint64_t seed      = 31;
            for (int index = 2; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument(
                        "Expected rollback-sim [--room N] [--players N] [--frames N] [--lag N] [--seed N]");
                if (std::string_view(argv[index]) == "--room")
                    roomId = static_cast<int32_t>(number(argv[index + 1]));
                else if (std::string_view(argv[index]) == "--players")
                    playerCount = static_cast<int>(number(argv[index + 1]));
                else if (std::string_view(argv[index]) == "--frames")
                    frames = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--lag")
                    lag = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--seed")
                    seed = number(argv[index + 1]);
                else
                    throw std::invalid_argument(
                        "Expected rollback-sim [--room N] [--players N] [--frames N] [--lag N] [--seed N]");
            }
            if (playerCount < 1 || playerCount > kMaxBattlePlayers)
                throw std::invalid_argument("--players must be between 1 and " + std::to_string(kMaxBattlePlayers));
            if (frames == 0)
                throw std::invalid_argument("--frames must be positive");
            return rollbackSim(roomId, playerCount, frames, lag, seed);
        }
        std::cerr
            << "usage: mugen_legacy_cli --version | trace-diff [--mode full|effects] [--allow file] <a.trace> <b.trace> | "
               "replay <file> [--hash-every N] [--compare hashfile] [--trace file] [--revision rev] [--ai on|off] | "
               "record-room <roomId> --frames N <file> [--ai on|off] | "
               "run-room <roomId> --frames N [--script file] [--trace file] [--revision rev] [--ai on|off] | "
               "bench-rollback [--room N] [--players N] [--samples N] [--warmup N] | "
               "rollback-sim [--room N] [--players N] [--frames N] [--lag N] [--seed N] | "
               "export-summary (--scenario <name> --crit min|max [--output path] | --all [--out-dir dir]) "
               "[--manifest path]\n"
               "script rows: frame slot controlEvent quadrant angleX1000 pressType\n";
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
