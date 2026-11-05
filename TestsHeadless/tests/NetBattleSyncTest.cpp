#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/net/LockstepAuthority.h"
#include "mugen/sim/net/NetBattleSync.h"
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace mugen;
using namespace mugen::sim;
using namespace mugen::sim::net;
using namespace mugen::sim::rollback;

namespace
{
constexpr int32_t kDuelRoom  = 30002;  // 纯 PvP 竞技场，无怪物、无警告。
constexpr uint64_t kTestSeed = 20261;

// 单帧全部槽位的脚本输入。
using FrameScript = std::array<PlayerInput, kMaxBattlePlayers>;

// 生成确定性、可复现的逐帧逐槽位输入脚本。
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
            else if (roll < 45)
                command.controlEvent = ControlEvent::CTR_RUN;
            else if (roll < 70)
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

// 构建 PVP 决斗世界配置：槽位 0 与其余槽位互为敌对阵营。
WorldSetup duelSetup(int playerCount, uint64_t seed)
{
    WorldSetup setup = test::ScenarioHarness::roomSetup(kDuelRoom, seed);
    setup.m_pvp      = true;
    for (int slot = 0; slot < playerCount; ++slot)
    {
        PlayerSetup p;
        p.m_slot   = static_cast<uint32_t>(slot);
        p.m_class  = CharacterClass::kSwordman;
        p.m_roleId = 101;
        p.m_level  = 100;
        p.m_camp   = slot == 0 ? 2 : 8;
        p.m_isAi   = false;
        setup.m_players.push_back(p);
    }
    return setup;
}

// 通过统一引导构建客户端世界，保证与权威世界初始状态一致。
World buildWorld(const WorldSetup& setup)
{
    World world;
    const auto heroes = bootstrapBattle(world, setup);
    if (heroes.empty())
        throw std::runtime_error("Cannot bootstrap duel world");
    return world;
}

// 确定性模拟延迟：槽位 0 通常准时、偶发迟到；其余槽位通常迟到一帧、偶发准时。
uint64_t deliveryLag(uint32_t slot, uint64_t frame)
{
    if (slot == 0)
        return frame % 47 == 0 ? 2 : 0;
    if (frame % (53 + slot) == 0)
        return 2;
    return frame % (31 + slot) == 0 ? 0 : 1;
}

// 运行权威世界并按延迟投递输入，返回每帧确认输入。
std::vector<FrameInput> runAuthority(LockstepAuthority& authority,
                                     const std::vector<FrameScript>& script,
                                     int playerCount,
                                     uint64_t frames)
{
    std::map<uint64_t, std::vector<std::pair<uint32_t, uint64_t>>> deliveries;
    for (uint64_t frame = 1; frame <= frames; ++frame)
        for (uint32_t slot = 0; slot < static_cast<uint32_t>(playerCount); ++slot)
            deliveries[frame + deliveryLag(slot, frame)].emplace_back(slot, frame);

    std::vector<FrameInput> confirmed(frames + 1);
    for (uint64_t frame = 1; frame <= frames; ++frame)
    {
        for (const auto& [slot, f] : deliveries[frame])
            authority.submitInput(slot, f, script[f - 1][slot]);
        confirmed[frame] = authority.step();
    }
    return confirmed;
}

// 以广播延迟消费确认输入，并领先预测本地槽位。
void driveClient(NetBattleSync& sync,
                 uint32_t localSlot,
                 const std::vector<FrameScript>& script,
                 const std::vector<FrameInput>& confirmed,
                 int playerCount,
                 uint64_t frames,
                 uint64_t bcastLag,
                 uint64_t lead)
{
    for (uint64_t step = 1; step <= frames + bcastLag; ++step)
    {
        const uint64_t f = step >= bcastLag + 1 ? step - bcastLag : 0;
        if (f >= 1 && f <= frames)
            for (uint32_t slot = 0; slot < static_cast<uint32_t>(playerCount); ++slot)
                sync.confirmInput(f, slot, confirmed[f].players[slot]);
        const uint64_t target = std::min<uint64_t>(step + lead, frames);
        if (step + lead <= frames)
            sync.submitLocalInput(target, script[target - 1][localSlot]);
        sync.session().advanceTo(target);
    }
    if (sync.session().currentFrame() == frames)
        sync.session().tick();
}
}  // namespace

TEST_CASE("NetBattleSync estimates server frame from clock offset")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSync sync(0, 2, {});
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);

    CHECK(sync.estimatedServerFrame(1000) == 0);
    CHECK(sync.estimatedServerFrame(1000 + 33 * 5) == 5);
    CHECK(sync.estimatedServerFrame(1000 + 33 * 5 + 10) == 5);  // 尚未到第 6 帧。

    // 目标帧 = 服务器帧 + 输入延迟。
    CHECK(sync.targetFrame(1000) == 2);
    CHECK(sync.targetFrame(1000 + 33 * 5) == 7);

    // 时钟偏移 EMA 平滑：首个样本为 0，后续样本按 1/10 逼近。
    CHECK(sync.clockOffsetMs() == 0);
    sync.onServerClock(10, 1330, 1400);  // 样本 = -70。
    CHECK(sync.clockOffsetMs() == -7);
}

TEST_CASE("NetBattleSync reconciles confirmed inputs and hashes against authority")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 300;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);

    NetBattleSync sync(0, playerCount, {});
    sync.start(buildWorld(setup), 0, 0);
    driveClient(sync, 0, script, confirmed, playerCount, frames, 2, 5);

    // 确认输入触发的回滚应发生。
    CHECK(sync.session().stats().rollbacks > 0);
    const uint64_t confirmedFrame = sync.session().confirmedFrame();
    CHECK(confirmedFrame >= frames);

    // 对账哈希帧不触发重同步。
    for (uint64_t h = kHashInterval; h <= confirmedFrame; h += kHashInterval)
        CHECK_FALSE(sync.checkServerHash(h, authority.hashAt(h)));

    // 收集的客户端哈希与权威一致。
    std::vector<std::pair<uint64_t, uint64_t>> clientHashes;
    sync.collectClientHashes(clientHashes);
    CHECK(!clientHashes.empty());
    for (const auto& [frame, hash] : clientHashes)
    {
        INFO("hash frame " << frame);
        CHECK(hash == authority.hashAt(frame));
    }
}

TEST_CASE("NetBattleSync flushRollback corrects hash before reconciliation")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 60;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);

    NetBattleSync sync(0, playerCount, {});
    sync.start(buildWorld(setup), 0, 0);

    // 先只预测本地槽位，不确认任何输入。
    for (uint64_t frame = 1; frame <= frames; ++frame)
    {
        sync.submitLocalInput(frame, script[frame - 1][0]);
        sync.session().advanceTo(frame);
    }
    // 一次性确认全部帧：远端槽位与迟到替换会产生待回滚。
    for (uint64_t frame = 1; frame <= frames; ++frame)
        for (uint32_t slot = 0; slot < playerCount; ++slot)
            sync.confirmInput(frame, slot, confirmed[frame].players[slot]);
    CHECK(sync.session().stats().mispredictions > 0);

    // 刷新待回滚后，确认帧哈希与权威一致。
    sync.flushRollback();
    const uint64_t c = sync.session().confirmedFrame();
    CHECK(c >= frames);
    CHECK(sync.session().hashAt(c) == authority.hashAt(c));
}

TEST_CASE("NetBattleSync resync recovers a desynced client")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 200;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);

    NetBattleSync sync(0, playerCount, {});
    sync.start(buildWorld(setup), 0, 0);
    driveClient(sync, 0, script, confirmed, playerCount, frames, 2, 5);
    CHECK(sync.session().hashAt(frames) == authority.hashAt(frames));

    // 制造失步：篡改角色位置。
    sync.session().world().ecs().each<Transform>(
        [](Entity, Transform& t) { t.position.x = t.position.x + Fixed::fromInt(1); });
    CHECK(sync.session().hash() != authority.hashAt(frames));

    // 采用权威当前快照复位。
    std::string snapshot;
    authority.serializeWorld(snapshot);
    ByteBuffer buffer;
    for (uint8_t b : snapshot)
        buffer.writeByte(b);
    buffer.writeFinish();
    CHECK(sync.resync(frames, buffer));
    CHECK(sync.session().hash() == authority.hashAt(frames));
}

TEST_CASE("NetBattleSync rejoin from a far-ahead snapshot realigns clock and frame target")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 200;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);
    (void)confirmed;

    // 重连：世界按开始参数引导（帧 0），时钟基于帧 0 时间戳，估计服务器帧停在 0 附近。
    constexpr uint64_t kStartServerMs = 100000;
    constexpr uint64_t kLocalNowMs    = 5000;
    NetBattleSync sync(1, playerCount, {});
    sync.start(buildWorld(setup), kStartServerMs, kLocalNowMs);
    CHECK(sync.estimatedServerFrame(kLocalNowMs) == 0);

    // 快照在帧 200，服务器此刻时间 = 帧 0 时间戳 + 200 帧。
    std::string snapshot;
    authority.serializeWorld(snapshot);
    ByteBuffer buffer;
    for (uint8_t b : snapshot)
        buffer.writeByte(b);
    buffer.writeFinish();
    REQUIRE(sync.resync(frames, buffer));
    sync.resetClock(kStartServerMs + frames * 33, kLocalNowMs);

    CHECK(sync.session().currentFrame() == frames);
    CHECK(sync.session().confirmedFrame() == frames);
    CHECK(sync.estimatedServerFrame(kLocalNowMs) == frames);
    // 目标帧立即位于快照帧之后（服务器帧 + 输入延迟），无需等待 EMA 收敛。
    CHECK(sync.targetFrame(kLocalNowMs) == frames + 2);
    CHECK(sync.session().hash() == authority.hashAt(frames));
}

TEST_CASE("LockstepAuthority releaseSlot drops queued input and idles the slot")
{
    test::ScenarioHarness::loadAssets();
    const auto setup = duelSetup(2, kTestSeed);
    LockstepAuthority authority(setup);

    PlayerInput run;
    run.slot = 1;
    PlayerCommand command;
    command.controlEvent = ControlEvent::CTR_RUN;
    command.quadrant     = ControlQuadrant::UpRight;
    command.pressType    = 1;
    run.commands.push_back(command);
    CHECK(authority.submitInput(1, 3, run));
    CHECK(authority.submitInput(1, 4, run));

    authority.releaseSlot(1);

    const auto& f1 = authority.step();
    REQUIRE(f1.players.size() == 2);
    REQUIRE(f1.players[1].commands.size() == 1);
    CHECK(f1.players[1].commands[0].controlEvent == ControlEvent::CTR_IDLE);
    authority.step();
    const auto f3 = authority.step();
    CHECK(f3.players[1].commands.empty());  // 已排队的跑动输入被丢弃。
    const auto f4 = authority.step();
    CHECK(f4.players[1].commands.empty());
}
