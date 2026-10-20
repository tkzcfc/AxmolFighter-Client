#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/Replay.h"
#include <cstring>
#include <cstdlib>
#include <filesystem>

namespace ecs_test
{
extern std::string executablePath;
}

using namespace mugen;
using namespace mugen::sim;

namespace
{
FrameInput frame(uint64_t value)
{
    return {value, {}, {}, {}};
}
std::vector<uint8_t> snapshot(const World& world)
{
    ByteBuffer buffer;
    world.serialize(buffer);
    return {buffer.data(), buffer.data() + buffer.len()};
}
void restore(World& world, std::vector<uint8_t> bytes)
{
    ByteBuffer buffer;
    for (uint8_t byte : bytes)
        buffer.writeByte(byte);
    buffer.writeFinish();
    REQUIRE(world.deserialize(buffer));
}
const auto& events(const World& world)
{
    return world.ecs().singleton<PresentationEvents>().events;
}
void emitGroup(World& world, Entity entity)
{
    world.emit(entity, Sound{static_cast<int32_t>(world.ecs().get<EntityGroup>(entity).kind)});
}
void removal(World& world, Entity entity)
{
    world.emit(entity, Sound{99});
}
void growingRole(World& world, Entity entity)
{
    emitGroup(world, entity);
    if (world.ecs().singleton<WorldClock>().frame == 1 && entity.index() == 0)
    {
        world.create(GroupKind::Role);
        world.create(GroupKind::Effect);
        world.destroy(entity);
    }
}
void scene(World& world)
{
    world.emit({}, Sound{100});
}
void collision(World& world)
{
    world.emit({}, Sound{101});
}
}  // namespace

TEST_CASE("World empty runs produce identical per-frame hashes for 1000 frames")
{
    test::ScenarioHarness first(test::ScenarioHarness::roomSetup(30002, 42));
    test::ScenarioHarness second(test::ScenarioHarness::roomSetup(30002, 42));
    CHECK(conf::Config::instance().loaded());
    CHECK(AvatarAssetCache::getInstance()->isLoaded());
    for (uint64_t n = 1; n <= 1000; ++n)
    {
        first.tick();
        second.tick();
        CHECK(first.world().hash() == second.world().hash());
        CHECK(first.world().hashSequence() == n);
    }
    CHECK(first.expectEntityCount(0));
    CHECK(first.expectHash(second.world().hash()));
}

TEST_CASE("World frame 500 byte snapshot restore and clone continue identically")
{
    World original;
    original.init({987, WorldMode::Town, 0, "town"});
    const Entity entity = original.create(GroupKind::Role);
    original.ecs().add(entity, ControlEventQueue{});
    original.ecs().singleton<PlayerSlots>().entities[3] = entity;
    original.ecs().singleton<LogicTimers>().add(entity, kLogicDtMs * Fixed::fromInt(700), false,
                                                EmitPresentation{Sound{7}});
    for (uint64_t n = 1; n <= 500; ++n)
    {
        original.ecs().singleton<RngState>().random.next();
        original.tick(frame(n));
    }
    const auto bytes = snapshot(original);
    World restored;
    restore(restored, bytes);
    CHECK(snapshot(restored) == bytes);
    World clone = original.clone();
    for (uint64_t n = 501; n <= 1000; ++n)
    {
        FrameInput input = frame(n);
        input.players.push_back({3, {{ControlEvent::CTR_WALK, ControlQuadrant::DownRight, 45000, 1}}});
        for (World* world : {&original, &restored, &clone})
        {
            world->ecs().singleton<RngState>().random.next();
            world->tick(input);
        }
        CHECK(original.hash() == restored.hash());
        CHECK(original.hash() == clone.hash());
    }
    clone.ecs().get<ControlEventQueue>(entity).commands.clear();
    CHECK(clone.hash() != original.hash());
    CHECK(original.ecs().get<ControlEventQueue>(entity).commands.size() == 500);
}

TEST_CASE("Timer exact frame loop remainder event sequence and unscaled clock")
{
    World world;
    world.init({});
    auto& timers = world.ecs().singleton<LogicTimers>();
    timers.add({}, kLogicDtMs * Fixed::fromInt(5), false, EmitPresentation{Sound{5}});
    timers.add({}, kLogicDtMs * Fixed::fromInt(5), true, EmitPresentation{Sound{10}});
    for (uint64_t n = 1; n <= 10; ++n)
    {
        auto input = frame(n);
        if (n == 1)
            input.systemCommands.push_back(SlowMotionCommand{Fixed::ratio(1, 2), 10});
        world.tick(input);
        if (n == 5)
        {
            REQUIRE(events(world).size() == 2);
            CHECK(events(world)[0].frame == 5);
            CHECK(events(world)[0].seq == 0);
            CHECK(events(world)[1].seq == 1);
        }
        else if (n == 10)
        {
            REQUIRE(events(world).size() == 1);
            CHECK(events(world)[0].seq == 2);
        }
        else
            CHECK(events(world).empty());
    }
    REQUIRE(timers.entries.size() == 1);
    CHECK(timers.entries[0].elapsedMs == Fixed{});
    CHECK(world.ecs().singleton<WorldClock>().dtMs == kLogicDtMs * Fixed::ratio(1, 2));
    timers.cancel(timers.entries[0].id);
    timers.add({}, Fixed::fromInt(10), true, EmitPresentation{Sound{1}});
    world.tick(frame(11));
    CHECK(events(world).size() == 1);
    CHECK(timers.entries[0].elapsedMs == kLogicDtMs - Fixed::fromInt(10));
    timers.resetTimes();
    CHECK(timers.entries[0].elapsedMs == Fixed{});
#if !defined(NDEBUG)
    const std::string executable = ecs_test::executablePath;
#    if defined(_WIN32)
    const std::string timerProbe = "\"" + executable + "\" --world-invalid-probe 0 > NUL 2>&1";
#    else
    const std::string timerProbe = "\"" + executable + "\" --world-invalid-probe 0 > /dev/null 2>&1";
#    endif
    CHECK(std::system(timerProbe.c_str()) != 0);
#endif
}

TEST_CASE("World groups remove in place and later groups capture their own bound")
{
    WorldHooks hooks;
    hooks.update.fill(emitGroup);
    hooks.update[0] = growingRole;
    hooks.remove.fill(removal);
    hooks.scene     = scene;
    hooks.collision = collision;
    World world(hooks);
    world.init({});
    const Entity first  = world.create(GroupKind::Role);
    const Entity second = world.create(GroupKind::Role);
    world.create(GroupKind::Portal);
    world.create(GroupKind::Goods);
    world.create(GroupKind::Obstacle);
    world.ecs().singleton<PlayerSlots>().entities[0] = first;
    world.ecs().singleton<LogicTimers>().add({}, Fixed{}, false, EmitPresentation{Sound{-1}});
    world.tick(frame(1));
    std::vector<int32_t> order;
    for (const auto& event : events(world))
        order.push_back(std::get<Sound>(event.payload).id);
    CHECK(order == std::vector<int32_t>{-1, 0, 99, 0, 1, 2, 3, 4, 100, 101});
    CHECK_FALSE(world.ecs().alive(first));
    CHECK(world.ecs().alive(second));
    CHECK(world.ecs().singleton<PlayerSlots>().entities.empty());
    world.tick(frame(2));
    CHECK(events(world).size() == 8);
}

TEST_CASE("World input queues preserve ordering and reject frames transactionally")
{
    test::ScenarioHarness harness({1, WorldMode::Town, 0, "town"});
    auto& world       = harness.world();
    const auto entity = world.create(GroupKind::Role);
    world.ecs().add(entity, ControlEventQueue{});
    world.ecs().singleton<PlayerSlots>().entities[4] = entity;
    REQUIRE(harness.inject(1, 4, {ControlEvent::CTR_WALK, ControlQuadrant::DownRight, 45000, 1}));
    REQUIRE(harness.inject(1, 4, {ControlEvent::CTR_NONE}));
    REQUIRE(harness.inject(1, 4, {ControlEvent::CTR_ATKA, ControlQuadrant::DownLeft, -90000, 2}));
    REQUIRE(harness.injectSystem(1, PauseCommand{true}));
    REQUIRE(harness.injectSystem(1, ReviveCommand{4}));
    harness.tick();
    const auto& commands = world.ecs().get<ControlEventQueue>(entity).commands;
    REQUIRE(commands.size() == 2);
    CHECK(commands[0].angle == 45000);
    CHECK(commands[1].controlEvent == ControlEvent::CTR_ATKA);
    CHECK(world.ecs().singleton<WorldClock>().dtMs == Fixed{});
    CHECK(world.ecs().singleton<sim::RoomState>().reviveRequests[0].slot == 4);
    const auto before = world.hash();
#if !defined(NDEBUG)
    const std::string executable = ecs_test::executablePath;
#    if defined(_WIN32)
    const std::string frameProbe = "\"" + executable + "\" --world-invalid-probe 1 > NUL 2>&1";
#    else
    const std::string frameProbe = "\"" + executable + "\" --world-invalid-probe 1 > /dev/null 2>&1";
#    endif
    CHECK(std::system(frameProbe.c_str()) != 0);
#endif
    CHECK(world.hash() == before);
    CHECK_FALSE(harness.inject(1, 4, {}));
    auto input = frame(2);
    input.systemCommands.push_back(PauseCommand{false});
    input.puppetUpdates.push_back({entity, {Fixed::fromInt(1), {}, Fixed::fromInt(3)}, -1, EntityRoleStatus::None});
    world.tick(input);
    CHECK(world.ecs().singleton<sim::RoomState>().puppetUpdates[0].position.z == Fixed::fromInt(3));
    CHECK(world.ecs().singleton<WorldClock>().dtMs == kLogicDtMs);
    CHECK(harness.expectEntityCount(1));
    CHECK_FALSE(harness.expectEntityCount(2));
    CHECK(harness.expectField(entity, &EntityGroup::kind, GroupKind::Role));
}

TEST_CASE("Timer destroy payload cancellation and World malformed snapshot")
{
    World world;
    world.init({});
    auto entity  = world.create(GroupKind::Effect);
    auto& timers = world.ecs().singleton<LogicTimers>();
    timers.add(entity, kLogicDtMs, false, DestroyEntity{entity});
    timers.add(entity, kLogicDtMs * Fixed::fromInt(2), false, EmitPresentation{Sound{9}});
    world.tick(frame(1));
    CHECK_FALSE(world.ecs().alive(entity));
    CHECK(timers.entries.empty());
    auto bytes = snapshot(world);
    bytes.pop_back();
    ByteBuffer buffer;
    for (uint8_t byte : bytes)
        buffer.writeByte(byte);
    buffer.writeFinish();
    const auto before = world.hash();
    CHECK_FALSE(world.deserialize(buffer));
    CHECK(world.hash() == before);
}

TEST_CASE("Replay canonical bytes roundtrip hashes and schema mismatch rejection")
{
    test::ScenarioHarness::loadAssets();
    Replay replay;
    replay.schemaHash = conf::configSchemaHash();
    replay.setup      = {321, WorldMode::Town, 0, "town"};
    for (uint64_t n = 1; n <= 1000; ++n)
    {
        auto input = frame(n);
        input.players.push_back({2, {{ControlEvent::CTR_WALK, ControlQuadrant::UpRight, 90000, 2}}});
        if (n == 5)
        {
            input.systemCommands = {ReviveCommand{2}, PauseCommand{false}, SlowMotionCommand{"0.5"_fx, 5}};
            input.puppetUpdates.push_back({{}, {"1"_fx, "2"_fx, "3"_fx}, -1, EntityRoleStatus::None});
        }
        replay.frames.push_back(std::move(input));
    }
    ByteBuffer bytes;
    replay.serialize(bytes);
    CHECK(std::memcmp(bytes.data(), "MGRP", 4) == 0);
    Replay decoded;
    std::string error;
    REQUIRE(decoded.deserialize(bytes, conf::configSchemaHash(), error));
    ByteBuffer again;
    decoded.serialize(again);
    REQUIRE(bytes.len() == again.len());
    CHECK(std::memcmp(bytes.data(), again.data(), bytes.len()) == 0);
    World first, second;
    first.init(replay.setup);
    second.init(decoded.setup);
    for (size_t n = 0; n < replay.frames.size(); ++n)
    {
        first.tick(replay.frames[n]);
        second.tick(decoded.frames[n]);
        CHECK(first.hash() == second.hash());
    }
    bytes.resetPosition();
    CHECK_FALSE(decoded.deserialize(bytes, conf::configSchemaHash() + 1, error));
    CHECK(error.find("schemaHash mismatch") != std::string::npos);
    CHECK(decoded.frames.size() == 1000);
    replay.frames[0].frame = 2;
    ByteBuffer invalid;
    replay.serialize(invalid);
    CHECK_FALSE(decoded.deserialize(invalid, conf::configSchemaHash(), error));
    CHECK(error.find("consecutive") != std::string::npos);
}
