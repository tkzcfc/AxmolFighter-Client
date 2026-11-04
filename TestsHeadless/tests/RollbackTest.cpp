#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/serialize/Hash.h"
#include "mugen/sim/rollback/RollbackSession.h"
#include "mugen/sim/room/RoomDirector.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include <array>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mugen;
using namespace mugen::sim;
using namespace mugen::sim::rollback;

namespace
{
constexpr int32_t kDuelRoom  = 30002;  // 纯 PvP 竞技场，无怪物、无警告，不产生系统命令。
constexpr uint64_t kTestSeed = 12345;

// 单帧全部槽位的脚本输入。
using FrameScript = std::array<PlayerInput, kMaxBattlePlayers>;

// 生成确定性、随机但可复现的逐帧逐槽位输入脚本。
std::vector<FrameScript> makeScript(uint64_t frames, int playerCount, uint64_t seed)
{
    std::mt19937 rng(static_cast<uint32_t>(seed));
    std::vector<FrameScript> script(frames);
    for (uint64_t frame = 1; frame <= frames; ++frame)
    {
        for (int slot = 0; slot < playerCount; ++slot)
        {
            PlayerInput& input  = script[frame - 1][slot];
            input.slot          = static_cast<uint32_t>(slot);
            const uint32_t roll = rng() % 100;
            PlayerCommand command;
            if (roll < 20)
                command.controlEvent = ControlEvent::CTR_ATKA;
            else if (roll < 42)
                command.controlEvent = ControlEvent::CTR_RUN;
            else if (roll < 66)
                command.controlEvent = ControlEvent::CTR_WALK;
            else
                command.controlEvent = ControlEvent::CTR_IDLE;
            command.quadrant  = static_cast<ControlQuadrant>(1 + (rng() % 4));
            command.angle     = (rng() % 2) == 0 ? 0 : 45000;
            command.pressType = 1;
            input.commands.push_back(command);
        }
    }
    return script;
}

// 建立含 playerCount 个互相敌对玩家并进入房间的战斗世界。
World buildDuelWorld(int32_t roomId, uint64_t seed, int playerCount)
{
    test::ScenarioHarness::loadAssets();
    World world;  // 默认 WorldHooks：Role 组走真实 role::update 战斗逻辑
    world.init(test::ScenarioHarness::roomSetup(roomId, seed));
    for (int slot = 0; slot < playerCount; ++slot)
    {
        RoleSpawnParams params;
        params.camp       = slot == 0 ? 2 : 1;  // 槽位 0 与其余槽位互为敌对
        params.level      = 100;
        params.isAi       = false;
        params.playerSlot = static_cast<uint32_t>(slot);
        if (RoleSpawner(world).spawnPlayer(CharacterClass::kSwordman, std::move(params)).value == 0)
            throw std::runtime_error("Cannot spawn player slot " + std::to_string(slot));
    }
    const auto* config = conf::Config::instance().find<conf::RoomConfig>(roomId);
    if (!config || !room::RoomDirector(world).enter(*config, 0, 0))
        throw std::runtime_error("Cannot enter room " + std::to_string(roomId));
    return world;
}

FrameInput scriptInput(uint64_t frame, const std::vector<FrameScript>& script, int playerCount)
{
    FrameInput input;
    input.frame = frame;
    for (int slot = 0; slot < playerCount; ++slot)
        input.players.push_back(script[frame - 1][slot]);
    return input;
}

// 参考世界逐帧推进，记录每帧哈希；可选在 snapshotFrame 处输出快照。
std::vector<uint64_t> runReference(const std::vector<FrameScript>& script,
                                   int playerCount,
                                   int32_t roomId,
                                   uint64_t seed,
                                   uint64_t snapshotFrame,
                                   ByteBuffer* snapshot)
{
    World world = buildDuelWorld(roomId, seed, playerCount);
    std::vector<uint64_t> hashes(script.size());
    for (uint64_t frame = 1; frame <= script.size(); ++frame)
    {
        world.tick(scriptInput(frame, script, playerCount));
        hashes[frame - 1] = world.hash();
        if (snapshot && frame == snapshotFrame)
            world.serialize(*snapshot);
    }
    return hashes;
}

// 以每槽位不同确认延迟驱动会话：本地槽位预测并回显确认，远端槽位延迟确认。
// 在全部槽位均已确认的帧上校验哈希与参考一致；结束时校验最终确认帧哈希。
void driveSession(RollbackSession& session,
                  const std::vector<FrameScript>& script,
                  int playerCount,
                  const std::vector<uint64_t>& lag,
                  const std::vector<uint64_t>& referenceHashes,
                  uint64_t startFrame,
                  uint64_t endFrame)
{
    const uint32_t localSlot = session.localSlot();
    for (uint64_t frame = startFrame; frame <= endFrame; ++frame)
    {
        session.addLocalInput(frame, script[frame - 1][localSlot]);
        for (int slot = 0; slot < playerCount; ++slot)
        {
            const uint64_t confirmed = frame >= lag[slot] ? frame - lag[slot] : 0;
            if (confirmed >= startFrame)
                session.addConfirmedInput(confirmed, static_cast<uint32_t>(slot), script[confirmed - 1][slot]);
        }
        CHECK(session.advanceTo(frame));
        if (frame <= session.confirmedFrame())
        {
            INFO("frame " << frame << " local slot " << localSlot);
            CHECK(session.hash() == referenceHashes[frame - 1]);
        }
    }
    const uint64_t confirmed = session.confirmedFrame();
    CHECK(confirmed > 0);
    CHECK(session.hashAt(confirmed) == referenceHashes[confirmed - 1]);
}

// 事件稳定标识，与会话内部 eventKey 一致，用于去重校验。
uint64_t testEventKey(const PresentationEvent& event)
{
    ByteBuffer buffer;
    BinaryWriter writer(buffer);
    writer.write(event.frame);
    writer.write(event.entity);
    writer.write(event.ordinal);
    writer.write(event.payload);
    writer.finish();
    return xxh64(buffer.data(), buffer.len());
}
}  // namespace

TEST_CASE("Rollback 2-player sessions match reference and roll back")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 200;
    const auto script         = makeScript(frames, playerCount, kTestSeed);
    const auto hashes         = runReference(script, playerCount, kDuelRoom, kTestSeed, 0, nullptr);

    const std::vector<std::vector<uint64_t>> lags = {{2, 6}, {7, 1}};
    uint64_t rollbackTotal                         = 0;
    for (uint32_t localSlot = 0; localSlot < static_cast<uint32_t>(playerCount); ++localSlot)
    {
        RollbackSession session(localSlot, playerCount);
        session.setWorld(buildDuelWorld(kDuelRoom, kTestSeed, playerCount));
        driveSession(session, script, playerCount, lags[localSlot], hashes, 1, frames);
        rollbackTotal += session.stats().rollbacks;
    }
    CHECK(rollbackTotal > 0);
}

TEST_CASE("Rollback 4-player sessions match reference")
{
    constexpr int playerCount = 4;
    constexpr uint64_t frames = 120;
    const auto script         = makeScript(frames, playerCount, kTestSeed);
    const auto hashes         = runReference(script, playerCount, kDuelRoom, kTestSeed, 0, nullptr);

    const std::vector<std::vector<uint64_t>> lags = {
        {1, 3, 5, 7},
        {4, 1, 6, 2},
        {7, 5, 3, 1},
        {2, 6, 1, 5},
    };
    uint64_t rollbackTotal = 0;
    for (uint32_t localSlot = 0; localSlot < static_cast<uint32_t>(playerCount); ++localSlot)
    {
        RollbackSession session(localSlot, playerCount);
        session.setWorld(buildDuelWorld(kDuelRoom, kTestSeed, playerCount));
        driveSession(session, script, playerCount, lags[localSlot], hashes, 1, frames);
        rollbackTotal += session.stats().rollbacks;
    }
    CHECK(rollbackTotal > 0);
}

TEST_CASE("Rollback stalls when prediction runs beyond snapshot depth")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 200;
    const auto script         = makeScript(frames, playerCount, kTestSeed);

    RollbackSession session(0, playerCount, 8);
    session.setWorld(buildDuelWorld(kDuelRoom, kTestSeed, playerCount));
    // 只提交本地预测，从不确认任何输入，确认帧恒为 0，预测在深度处被阻塞。
    for (uint64_t frame = 1; frame <= frames; ++frame)
        session.addLocalInput(frame, script[frame - 1][0]);
    CHECK_FALSE(session.advanceTo(frames));
    CHECK(session.currentFrame() == 8);
    CHECK(session.stats().stalls > 0);
}

TEST_CASE("Rollback resync recovers a desynced session")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 160;
    constexpr uint64_t reset  = 80;
    const auto script         = makeScript(frames, playerCount, kTestSeed);

    ByteBuffer snapshot;
    const auto hashes = runReference(script, playerCount, kDuelRoom, kTestSeed, reset, &snapshot);

    RollbackSession session(0, playerCount);
    session.setWorld(buildDuelWorld(kDuelRoom, kTestSeed, playerCount));
    driveSession(session, script, playerCount, {2, 6}, hashes, 1, reset);

    // 直接篡改一个角色的位置以制造失步。
    session.world().ecs().each<Transform>(
        [](Entity, Transform& transform) { transform.position.x = transform.position.x + Fixed::fromInt(1); });
    CHECK(session.hash() != hashes[reset - 1]);

    // 采用参考快照复位并继续匹配。
    CHECK(session.resync(reset, snapshot));
    CHECK(session.currentFrame() == reset);
    CHECK(session.hash() == hashes[reset - 1]);
    driveSession(session, script, playerCount, {2, 6}, hashes, reset + 1, frames);
}

TEST_CASE("Rollback presentation events have no duplicate identical events")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 200;
    const auto script         = makeScript(frames, playerCount, kTestSeed);
    const auto hashes         = runReference(script, playerCount, kDuelRoom, kTestSeed, 0, nullptr);

    RollbackSession session(0, playerCount);
    session.setWorld(buildDuelWorld(kDuelRoom, kTestSeed, playerCount));
    driveSession(session, script, playerCount, {2, 6}, hashes, 1, frames);
    CHECK(session.stats().rollbacks > 0);

    std::set<uint64_t> seen;
    size_t eventCount = 0;
    for (const auto& event : session.eventsToPresent())
    {
        CHECK(seen.insert(testEventKey(event)).second);
        ++eventCount;
    }
    CHECK(eventCount > 0);
}
