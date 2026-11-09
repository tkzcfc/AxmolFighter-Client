#include "doctest.h"
#include "mugen/sim/Schedule.h"
#include "mugen/sim/World.h"

#include <set>
#include <string>
#include <vector>

using namespace mugen;
using namespace mugen::sim;

namespace
{
// 构造一个空世界配置。
WorldSetup emptySetup()
{
    WorldSetup setup;
    setup.seed = 7;
    return setup;
}
// 推进一帧空输入。
void tickEmpty(World& world)
{
    FrameInput input;
    input.frame = world.frame() + 1;
    world.tick(input);
}
// 跑 frames 帧并记录每帧哈希。
std::vector<uint64_t> runHashes(uint32_t frames)
{
    World world;
    world.init(emptySetup());
    std::vector<uint64_t> hashes;
    for (uint32_t i = 0; i < frames; ++i)
    {
        tickEmpty(world);
        hashes.push_back(world.hash());
    }
    return hashes;
}
}  // namespace

TEST_CASE("sim empty world is deterministic over 1000 frames")
{
    const auto a = runHashes(1000);
    const auto b = runHashes(1000);
    CHECK(a == b);
    CHECK(a.front() != a.back());
}

TEST_CASE("sim clock advances frame and milliseconds")
{
    World world;
    world.init(emptySetup());
    for (int i = 0; i < 30; ++i)
        tickEmpty(world);
    CHECK(world.frame() == 30);
    CHECK(world.ecs().singleton<WorldClock>().m_elapsedMs == kLogicDtMs * Fixed::fromInt(30));
}

TEST_CASE("sim snapshot round trip and clone stay equal")
{
    World world;
    world.init(emptySetup());
    for (int i = 0; i < 50; ++i)
        tickEmpty(world);
    ByteBuffer buffer;
    world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    World cloned = world.clone();
    CHECK(restored.hash() == world.hash());
    CHECK(cloned.hash() == world.hash());
    for (int i = 0; i < 100; ++i)
    {
        tickEmpty(world);
        tickEmpty(restored);
        tickEmpty(cloned);
        CHECK(restored.hash() == world.hash());
        CHECK(cloned.hash() == world.hash());
    }
}

TEST_CASE("sim snapshot rejects truncated data and keeps the world")
{
    World world;
    world.init(emptySetup());
    tickEmpty(world);
    ByteBuffer buffer;
    world.serialize(buffer);
    ByteBuffer truncated;
    for (uint32_t i = 0; i + 1 < buffer.len(); ++i)
        truncated.writeByte(buffer.data()[i]);
    truncated.writeFinish();
    const uint64_t before = world.hash();
    CHECK_FALSE(world.deserialize(truncated));
    CHECK(world.hash() == before);
}

TEST_CASE("sim schedule table is ordered unique and complete")
{
    std::set<std::string> names;
    uint8_t phase = 0;
    for (const sim::SystemEntry& entry : kSchedule)
    {
        CHECK(entry.m_phase >= phase);
        phase = entry.m_phase;
        CHECK(entry.m_run != nullptr);
        CHECK(names.insert(entry.m_name).second);
    }
    const std::vector<std::string> expected = {
        "ClockSystem",           "ScheduledActionSystem",  "InputSystem",           "CooldownSystem",
        "BuffTickSystem",        "VitalsTickSystem",       "PauseTickSystem",       "AiSystem",
        "PlayerIntentSystem",    "SkillRequestSystem",     "RoleFsmSystem",         "TimelineSystem(roles)",
        "SpawnFlush(RoleTimeline)", "TimelineSystem(effects)", "SpawnFlush(EffectTimeline)", "HitReactionSystem",
        "LocomotionSystem",      "DisplacementSystem",     "FollowSystem",          "AnimationSystem",
        "HitboxSystem",          "CollisionSystem",        "CombatSystem",          "ObstacleSystem",
        "InteractSystem",        "SpawnFlush(Combat)",     "DeathSystem",           "TransformSystem",
        "DespawnSystem",         "SpawnFlush(Lifecycle)",  "RoomDirectorSystem",    "SpawnFlush(Room)",
        "PresentationSystem",    "DebugSink",              "RoomTransferSystem",    "FlushSystem"};
    REQUIRE(kSchedule.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK(expected[i] == kSchedule[i].m_name);
    CHECK(kSchedule.front().m_phase == static_cast<uint8_t>(Phase::FrameBegin));
    CHECK(kSchedule.back().m_phase == static_cast<uint8_t>(Phase::FrameEnd));
}

TEST_CASE("sim event queue push clear and snapshot round trip")
{
    EventQueue<DeathEvent> queue;
    CHECK(queue.empty());
    Entity a;
    a.value = 5;
    queue.push(DeathEvent{a, a});
    queue.push(DeathEvent{});
    CHECK(queue.size() == 2);
    queue.clear();
    CHECK(queue.empty());

    World world;
    world.init(emptySetup());
    SpawnRequest request;
    request.m_configId = 9;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
    world.ecs().singleton<EventQueue<HitEvent>>().push(HitEvent{});
    const uint64_t withEvents = world.hash();
    ByteBuffer buffer;
    world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    CHECK(restored.hash() == withEvents);
    CHECK(restored.ecs().singleton<EventQueue<SpawnRequest>>().m_items.at(0).m_configId == 9);
}

TEST_CASE("sim frame end flushes destroyed entities and clears fact queues")
{
    World world;
    world.init(emptySetup());
    const Entity e = world.ecs().create();
    world.ecs().add(e, Despawning{});
    world.ecs().destroy(e);
    world.ecs().singleton<EventQueue<HitEvent>>().push(HitEvent{});
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(SpawnRequest{});
    tickEmpty(world);
    CHECK_FALSE(world.ecs().alive(e));
    CHECK(world.ecs().singleton<EventQueue<HitEvent>>().empty());
    CHECK(world.ecs().singleton<EventQueue<SpawnRequest>>().empty());
}
