#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/conf/ConfigFormat.h"
#include "mugen/sim/Replay.h"
#include "mugen/sim/buff/BuffLifecycle.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace mugen;
using namespace mugen::sim;

namespace
{
std::filesystem::path replayPath(std::string_view name)
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "data/replays" / name;
}

Replay loadReplay(std::string_view name)
{
    Replay replay;
    std::string error;
    REQUIRE(replay.load(replayPath(name).string(), conf::configSchemaHash(), error));
    return replay;
}

World replayWorld(const Replay& replay)
{
    return test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
}

std::vector<uint8_t> bytes(const World& world)
{
    ByteBuffer buffer;
    world.serialize(buffer);
    return {buffer.data(), buffer.data() + buffer.len()};
}

World restored(const std::vector<uint8_t>& snapshot)
{
    ByteBuffer buffer;
    for (const auto value : snapshot)
        buffer.writeByte(value);
    buffer.writeFinish();
    World world(test::ScenarioHarness::roomHooks());
    REQUIRE(world.deserialize(buffer));
    return world;
}
}  // namespace

TEST_CASE("Determinism replay runs twice identical")
{
    for (const auto name : {"copy101_room2010101.mgrp", "defense2071501.mgrp", "arena30002.mgrp"})
    {
        const auto replay = loadReplay(name);
        auto first        = replayWorld(replay);
        auto second       = replayWorld(replay);
        for (const auto& input : replay.frames)
        {
            first.tick(input);
            second.tick(input);
            CHECK(first.hash() == second.hash());
        }
    }
}

TEST_CASE("Determinism snapshot restore at 1 137 900")
{
    const auto replay = loadReplay("copy101_room2010101.mgrp");
    constexpr std::array<size_t, 3> snapshots{1, 137, 900};
    auto original = replayWorld(replay);
    for (size_t index = 0; index < replay.frames.size(); ++index)
    {
        original.tick(replay.frames[index]);
        if (std::find(snapshots.begin(), snapshots.end(), index + 1) != snapshots.end())
        {
            auto copy = restored(bytes(original));
            for (size_t next = index + 1; next < replay.frames.size(); ++next)
            {
                original.tick(replay.frames[next]);
                copy.tick(replay.frames[next]);
                CHECK(original.hash() == copy.hash());
            }
            original = replayWorld(replay);
            for (size_t replayIndex = 0; replayIndex <= index; ++replayIndex)
                original.tick(replay.frames[replayIndex]);
        }
    }
}

TEST_CASE("Determinism clone rollback 60 frames")
{
    const auto replay = loadReplay("defense2071501.mgrp");
    auto original     = replayWorld(replay);
    for (size_t index = 0; index < 500; ++index)
        original.tick(replay.frames[index]);
    auto clone          = original.clone();
    const auto snapshot = bytes(clone);
    for (size_t index = 500; index < 560; ++index)
    {
        original.tick(replay.frames[index]);
        clone.tick(replay.frames[index]);
        CHECK(original.hash() == clone.hash());
    }
    clone         = restored(snapshot);
    auto replayed = restored(snapshot);
    for (size_t index = 500; index < 560; ++index)
    {
        clone.tick(replay.frames[index]);
        replayed.tick(replay.frames[index]);
        CHECK(clone.hash() == replayed.hash());
    }
}

TEST_CASE("Determinism serialization roundtrip bytes")
{
    const auto replay = loadReplay("arena30002.mgrp");
    ByteBuffer first;
    replay.serialize(first);
    Replay decoded;
    std::string error;
    REQUIRE(decoded.deserialize(first, conf::configSchemaHash(), error));
    ByteBuffer second;
    decoded.serialize(second);
    REQUIRE(first.len() == second.len());
    CHECK(std::memcmp(first.data(), second.data(), first.len()) == 0);
}

TEST_CASE("Golden buff tick timing")
{
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path() / "golden/buff_tick_golden.csv");
    REQUIRE(file.good());
    buff::Lifecycle lifecycle;
    lifecycle.m_active      = true;
    lifecycle.m_maxInterval = Fixed::fromInt(1);
    lifecycle.m_maxTimes    = -1;
    std::string line;
    int rows = 0;
    while (std::getline(file, line))
    {
        if (line.empty() || line.front() == '#')
            continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        int frame        = 0;
        int expectedTick = 0;
        REQUIRE(static_cast<bool>(row >> frame >> expectedTick));
        CAPTURE(frame);
        CHECK(buff::advance(lifecycle, kLogicDtSec) ==
              (expectedTick != 0 ? buff::LifecycleAction::Update : buff::LifecycleAction::None));
        ++rows;
    }
    CHECK(rows == 65);
}
