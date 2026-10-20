#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/conf/ConfigFormat.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/Replay.h"
#include "mugen/sim/World.h"
#include "mugen/sim/room/RoomDirector.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/trace/TraceSink.h"
#include <filesystem>

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
}  // namespace

TEST_CASE("Trace same replay twice is identical")
{
    const auto replay = loadReplay("defense2071501.mgrp");
    World first       = test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
    World second      = test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
    trace::TraceSink a, b;
    first.setTrace(&a);
    second.setTrace(&b);
    for (const auto& input : replay.frames)
    {
        first.tick(input);
        second.tick(input);
    }
    REQUIRE_FALSE(a.text().empty());
    CHECK(a.text() == b.text());
}

TEST_CASE("Trace clone mid-run continues identically")
{
    const auto replay = loadReplay("arena30002.mgrp");
    World original    = test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
    const size_t half = replay.frames.size() / 2;
    for (size_t index = 0; index < half; ++index)
        original.tick(replay.frames[index]);
    World clone = original.clone();
    trace::TraceSink a, b;
    original.setTrace(&a);
    clone.setTrace(&b);
    for (size_t index = half; index < replay.frames.size(); ++index)
    {
        original.tick(replay.frames[index]);
        clone.tick(replay.frames[index]);
    }
    REQUIRE_FALSE(a.text().empty());
    CHECK(a.text() == b.text());
}

TEST_CASE("Trace two worlds with separate sinks do not interleave")
{
    const auto replay = loadReplay("copy101_room2010101.mgrp");
    World first       = test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
    World second      = test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
    trace::TraceSink a, b;
    first.setTrace(&a);
    second.setTrace(&b);
    // 交替推进两个世界，各自追踪文本应互不干扰且与单独推进一致。
    for (const auto& input : replay.frames)
    {
        first.tick(input);
        second.tick(input);
    }
    CHECK(a.text() == b.text());

    trace::TraceSink solo;
    World only = test::ScenarioHarness::roomWorld(replay.setup.roomId, replay.setup.seed);
    only.setTrace(&solo);
    for (const auto& input : replay.frames)
        only.tick(input);
    CHECK(a.text() == solo.text());
}

TEST_CASE("Trace combat scenario captures skill cast and is deterministic")
{
    test::ScenarioHarness::loadAssets();
    const int32_t roomId = 2071501;
    auto build           = [&]() {
        World world;  // 默认 WorldHooks，Role 组走真实 role::update
        world.init(test::ScenarioHarness::roomSetup(roomId, 31));
        sim::RoleSpawnParams player;
        player.camp       = 2;
        player.playerSlot = 0;
        sim::RoleSpawner(world).spawnPlayer(CharacterClass::kSwordman, std::move(player));
        const auto* config = conf::Config::instance().find<conf::RoomConfig>(roomId);
        REQUIRE(config);
        REQUIRE(sim::room::RoomDirector(world).enter(*config, 0, 0));
        return world;
    };
    auto drive = [](World& world) {
        for (uint64_t frame = 1; frame <= 60; ++frame)
        {
            FrameInput input{frame, {}, {}, {}};
            if (frame >= 5 && frame <= 30)
                input.players.push_back({0, {{ControlEvent::CTR_RUN, ControlQuadrant::DownLeft, 0, 1}}});
            else if (frame >= 31 && frame <= 50)
                input.players.push_back({0, {{ControlEvent::CTR_ATKA, ControlQuadrant::DownLeft, 0, 1}}});
            world.tick(input);
        }
    };
    World first  = build();
    World second = build();
    trace::TraceSink a, b;
    first.setTrace(&a);
    second.setTrace(&b);
    drive(first);
    drive(second);
    REQUIRE_FALSE(a.text().empty());
    CHECK(a.text() == b.text());
    CHECK(a.text().find(" sk ") != std::string::npos);
}
