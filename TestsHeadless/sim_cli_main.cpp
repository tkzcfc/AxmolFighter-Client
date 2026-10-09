#include "support/ScenarioHarness.h"
#include "mugen/conf/ConfigFormat.h"
#include "mugen/sim/Replay.h"
#include <charconv>
#include <cstdint>
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
            for (int index = 3; index < argc; index += 2)
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument("Expected replay <file> [--hash-every N] [--compare hashfile]");
                if (std::string_view(argv[index]) == "--hash-every")
                    every = number(argv[index + 1]);
                else if (std::string_view(argv[index]) == "--compare")
                    compare = argv[index + 1];
                else
                    throw std::invalid_argument("Expected replay <file> [--hash-every N] [--compare hashfile]");
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
                throw std::invalid_argument("Expected run-room <roomId> --frames N [--script file]");
            const auto frames = number(argv[4]);
            std::map<uint64_t, FrameInput> inputs;
            if (argc == 7 && std::string_view(argv[5]) == "--script")
                inputs = loadScript(argv[6], frames);
            else if (argc != 5)
                throw std::invalid_argument("Expected run-room <roomId> --frames N [--script file]");
            test::ScenarioHarness harness(test::ScenarioHarness::roomSetup(static_cast<int32_t>(room)));
            for (uint64_t frame = 1; frame <= frames; ++frame)
            {
                auto input = inputs.extract(frame);
                harness.world().tick(input.empty() ? FrameInput{frame, {}, {}, {}} : input.mapped());
            }
            printHash(harness.world());
            return 0;
        }
        std::cerr << "usage: mugen_sim_cli --version | replay <file> [--hash-every N] [--compare hashfile] | "
                     "record-room <roomId> --frames N <file> | "
                     "run-room <roomId> --frames N [--script file]\n"
                     "script rows: frame slot controlEvent quadrant angleX1000 pressType\n";
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
