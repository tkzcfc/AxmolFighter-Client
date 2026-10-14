#include "support/ScenarioHarness.h"
#include "mugen/conf/ConfigFormat.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/Replay.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/trace/TraceSink.h"
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace
{
using namespace mugen;
using namespace mugen::sim;
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
        PlayerCommand command;
        std::string extra;
        if (!(row >> frame >> slot >> event >> command.quadrant >> command.angle >> command.pressType) ||
            (row >> extra) || frame == 0 || frame > frames || event > 255)
            throw std::invalid_argument("Invalid command script line " + std::to_string(lineNumber));
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
    if (setup.mode == WorldMode::Battle && setup.roomId > 0)
        return test::ScenarioHarness::roomWorld(setup.roomId, setup.seed);
    World world;
    world.init(setup);
    return world;
}
FrameInput recordedInput(World& world)
{
    FrameInput input;
    input.frame      = world.ecs().singleton<WorldClock>().frame + 1;
    const auto& room = world.ecs().singleton<mugen::sim::RoomState>();
    if (room.m_state == mugen::RoomState::ReadySpine)
        input.systemCommands.push_back(RoomWarningDone{room.roomId});
    PlayerCommand command;
    command.controlEvent = room.m_victory ? ControlEvent::CTR_RUN
                                           : (input.frame % 30 == 0 ? ControlEvent::CTR_ATKA : ControlEvent::CTR_WALK);
    command.quadrant     = 1;
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
    world.init(test::ScenarioHarness::roomSetup(roomId, seed));
    sim::RoleSpawnParams player;
    player.camp       = 2;
    player.playerSlot = 0;
    player.level      = level;
    sim::spawnPlayer(world, static_cast<CharacterClass>(occupation), std::move(player));
    const auto* config = conf::Config::instance().find<conf::RoomConfig>(roomId);
    if (!config || !sim::room::enter(world, *config, 0, 0))
        throw std::runtime_error("Cannot enter room " + std::to_string(roomId));
    if (std::getenv("MUGEN_TRACE_DUMP_SKILLS"))
    {
        world.ecs().each<RoleIdentity, SkillPool, Vitals>([&](Entity, const RoleIdentity& id, const SkillPool& pool,
                                                               const Vitals& vitals) {
            std::cerr << "[dump] role " << id.roleId << " ep=" << vitals.ep.toInt() << "/" << vitals.maxEp.toInt()
                      << " crystals=" << pool.m_crystals << '\n';
            for (size_t s = 0; s < pool.m_slots.size(); ++s)
            {
                std::cerr << "  slot '" << pool.m_slots[s].m_name << "':";
                for (const auto& idx : pool.m_index)
                    if (idx.m_address.m_slot == s)
                    {
                        const auto& sk = pool.m_skills[idx.m_skill];
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
}  // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--version")
        {
            std::cout << "mugen_sim_cli 0.1.0\n";
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[1]) == "replay")
        {
            uint64_t every = 1;
            std::string compare;
            std::string tracePath;
            std::string revision;
            for (int index = 3; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument(
                        "Expected replay <file> [--hash-every N] [--compare hashfile] [--trace file] [--revision rev]");
                if (std::string_view(argv[index]) == "--hash-every")
                    every = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--compare")
                    compare = argv[index + 1];
                else if (std::string_view(argv[index]) == "--trace")
                    tracePath = argv[index + 1];
                else if (std::string_view(argv[index]) == "--revision")
                    revision = argv[index + 1];
                else
                    throw std::invalid_argument(
                        "Expected replay <file> [--hash-every N] [--compare hashfile] [--trace file] [--revision rev]");
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
        if (argc == 6 && std::string_view(argv[1]) == "record-room" && std::string_view(argv[3]) == "--frames")
        {
            const auto roomId = number(argv[2]);
            const auto frames = number(argv[4]);
            if (roomId > INT32_MAX || frames == 0 || std::string_view(argv[5]).empty())
                throw std::invalid_argument("Expected record-room <roomId> --frames N <file>");
            Replay replay;
            replay.schemaHash = conf::configSchemaHash();
            replay.setup      = test::ScenarioHarness::roomSetup(static_cast<int32_t>(roomId), 31);
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
                    "Expected run-room <roomId> --frames N [--class 1-4] [--seed N] [--script file] [--trace file] "
                    "[--revision rev]");
            const auto frames = number(argv[4]);
            std::map<uint64_t, FrameInput> inputs;
            std::string tracePath;
            std::string revision;
            int32_t occupation = 0;
            uint64_t seed      = 31;
            int32_t level      = 1;
            for (int index = 5; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument(
                        "Expected run-room <roomId> --frames N [--class 1-4] [--seed N] [--level N] [--script file] "
                        "[--trace file] [--revision rev]");
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
                else
                    throw std::invalid_argument(
                        "Expected run-room <roomId> --frames N [--class 1-4] [--seed N] [--level N] [--script file] "
                        "[--trace file] [--revision rev]");
            }
            World world;
            if (occupation >= 1 && occupation <= 4)
                world = combatWorld(static_cast<int32_t>(room), seed, occupation, level);
            else
                world.init(test::ScenarioHarness::roomSetup(static_cast<int32_t>(room), seed));
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
            const std::string first = argv[2];
            const std::string second = argv[3];
            std::ifstream fa(first);
            std::ifstream fb(second);
            if (!fa)
                throw std::runtime_error("Cannot open trace file: " + first);
            if (!fb)
                throw std::runtime_error("Cannot open trace file: " + second);
            std::string la, lb;
            uint64_t lineNumber = 0;
            int printed = 0;
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
        std::cerr << "usage: mugen_sim_cli --version | trace-diff <a.trace> <b.trace> | "
                     "replay <file> [--hash-every N] [--compare hashfile] [--trace file] [--revision rev] | "
                     "record-room <roomId> --frames N <file> | "
                     "run-room <roomId> --frames N [--script file] [--trace file] [--revision rev]\n"
                     "script rows: frame slot controlEvent quadrant angleX1000 pressType\n";
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
