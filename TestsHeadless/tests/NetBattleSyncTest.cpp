#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/net/LockstepAuthority.h"
#include "mugen/sim/net/NetBattleSync.h"
#include "mugen/render/battle/PoseTrack.h"
#include <array>
#include <cstdint>
#include <map>
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

namespace
{
// 构造一条首次到达输入的反馈样本。
ServerInputFeedback slackSample(int32_t slack, uint64_t frame, uint64_t echoMs = 0, uint32_t late = 0)
{
    ServerInputFeedback fb;
    fb.m_slackMin     = slack;
    fb.m_slackFrame   = frame;
    fb.m_slackSamples = 2;
    fb.m_echoClientTimeMs = echoMs;
    fb.m_lateInputs   = late;
    return fb;
}

PlayerInput atkInput(uint32_t slot)
{
    PlayerInput in;
    in.slot = slot;
    in.commands.push_back({ControlEvent::CTR_ATKA, ControlQuadrant::None, 0, 1});
    return in;
}

PlayerInput runInput(uint32_t slot)
{
    PlayerInput in;
    in.slot = slot;
    in.commands.push_back({ControlEvent::CTR_RUN, ControlQuadrant::UpRight, 0, 1});
    return in;
}
}  // namespace

TEST_CASE("NetBattleSync measures RTT from the echoed client timestamp")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSync sync(0, 2, {});
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);
    CHECK(sync.rttMs() == 0);

    sync.onServerFeedback(slackSample(5, 10, /*echo*/ 2000), 2080);  // 首个样本 80ms。
    CHECK(sync.rttMs() == 80);
    CHECK(sync.rttJitterMs() == 40);
    sync.onServerFeedback(slackSample(5, 12, /*echo*/ 3000), 3160);  // 160ms：EMA 1/8 逼近。
    CHECK(sync.rttMs() == 90);
    // 无回显或回显时间在未来的样本被忽略。
    sync.onServerFeedback(slackSample(5, 14, 0), 4000);
    sync.onServerFeedback(slackSample(5, 16, 9000), 4000);
    CHECK(sync.rttMs() == 90);
}

TEST_CASE("NetBattleSync grows the input lead fast and ignores stale feedback")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSyncConfig config;
    config.m_inputDelayFrames = 4;
    NetBattleSync sync(0, 2, config);
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);
    CHECK(sync.inputLeadFrames() == 4);
    CHECK(sync.targetFrame(1000) == 4);

    // 富余充足：不变。
    sync.onServerFeedback(slackSample(3, 5), 1100);
    CHECK(sync.inputLeadFrames() == 4);

    // 迟到 3 帧（富余 -3）：一次补足缺口 = 目标富余 2 - (-3) = 5。
    sync.onServerFeedback(slackSample(-3, 6, 0, 7), 1200);
    CHECK(sync.inputLeadFrames() == 9);
    CHECK(sync.lateInputs() == 7);
    CHECK(sync.lastSlackFrames() == -3);
    CHECK(sync.targetFrame(1000) == 9);

    // 领先量变化前发出的帧（帧号小于生效帧 1）已无参考价值；用推进过的会话模拟：先推进 20 帧再变化。
    for (uint64_t f = 1; f <= 20; ++f)
    {
        sync.submitLocalInput(f, PlayerInput{});
        sync.session().advanceTo(f);
    }
    sync.onServerFeedback(slackSample(-10, 30), 1300);  // 帧 30 ≥ 生效帧 1：采纳；单次最多 +8。
    CHECK(sync.inputLeadFrames() == 17);
    // 新领先量下的生效帧 = max(当前帧 + 1, 估计服务器帧 + 新领先量 - 目标富余) = 9 + 17 - 2 = 24：
    // 途中仍在迟到的旧帧与追帧期间的帧（帧号 < 24）被忽略，避免连续超调。
    sync.onServerFeedback(slackSample(-10, 21), 1400);
    CHECK(sync.inputLeadFrames() == 17);
    sync.onServerFeedback(slackSample(-10, 24), 1500);
    CHECK(sync.inputLeadFrames() == 25);
    // 受上限封顶：回滚窗口 120 与服务器接受上限 90 之内，且不超过配置的 30。
    sync.onServerFeedback(slackSample(-30, 40), 1600);
    sync.onServerFeedback(slackSample(-30, 50), 1700);
    CHECK(sync.inputLeadFrames() == config.m_maxLeadFrames);
}

TEST_CASE("NetBattleSync shrinks the input lead slowly with hysteresis and an RTT floor")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSyncConfig config;
    config.m_inputDelayFrames = 12;
    const auto setup          = duelSetup(2, kTestSeed);

    // 富余在滞回带内（目标 2 + 余量 2 = 4 以下）：不缩小。
    {
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        CHECK(sync.inputLeadFrames() == 12);
        for (uint64_t f = 1; f <= 200; f += 5)
            sync.onServerFeedback(slackSample(3, f), 1000);
        CHECK(sync.inputLeadFrames() == 12);
    }
    // 窗口内最小富余 = 4：满足滞回，每个窗口（30 帧）只缩小 1 帧。
    {
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        for (uint64_t f = 1; f <= 36; f += 5)  // 恰好一个窗口。
            sync.onServerFeedback(slackSample(4, f), 1000);
        CHECK(sync.inputLeadFrames() == 11);
        for (uint64_t f = 41; f <= 200; f += 5)
            sync.onServerFeedback(slackSample(4, f), 1000);
        CHECK(sync.inputLeadFrames() < 11);
        CHECK(sync.inputLeadFrames() >= 12 - 5);  // 160 帧至多 5 个窗口，每窗口 1 帧。
    }
    // 富余很大时缩得更快（约三分之一的富余），但仍不低于配置下限。
    {
        NetBattleSync slow(0, 2, config);
        NetBattleSync fast(0, 2, config);
        slow.start(buildWorld(setup), 1000, 1000);
        fast.start(buildWorld(setup), 1000, 1000);
        for (uint64_t f = 1; f <= 36; f += 5)
        {
            slow.onServerFeedback(slackSample(4, f), 1000);
            fast.onServerFeedback(slackSample(16, f), 1000);
        }
        CHECK(slow.inputLeadFrames() == 11);
        CHECK(fast.inputLeadFrames() < slow.inputLeadFrames());
        for (uint64_t f = 41; f <= 600; f += 5)
            fast.onServerFeedback(slackSample(100, f), 1000);
        CHECK(fast.inputLeadFrames() == config.m_minLeadFrames);
    }
    // 窗口内只要反复出现低于滞回线的样本，就不缩小。
    {
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        for (uint64_t f = 1; f <= 400; f += 5)
            sync.onServerFeedback(slackSample(f % 10 == 1 ? 3 : 20, f), 1000);
        CHECK(sync.inputLeadFrames() == 12);
    }
    // RTT 下限：测得 300ms RTT（含抖动约 19 帧）后，不会缩到该帧数以下；无 RTT 时则可缩到最小。
    {
        NetBattleSyncConfig high = config;
        high.m_inputDelayFrames  = 24;
        NetBattleSync withRtt(0, 2, high);
        NetBattleSync noRtt(0, 2, high);
        withRtt.start(buildWorld(setup), 1000, 1000);
        noRtt.start(buildWorld(setup), 1000, 1000);
        withRtt.onServerFeedback(slackSample(20, 1, 2000), 2300);  // RTT 300ms，偏差 150ms。
        for (uint64_t f = 2; f <= 800; f += 5)
        {
            withRtt.onServerFeedback(slackSample(20, f), 2300);
            noRtt.onServerFeedback(slackSample(20, f), 2300);
        }
        // 300ms 的 RTT 触发 1 帧本地输入延迟 D，领先量只需覆盖其余部分：领先量 + D 恒不低于 RTT 所需帧数。
        CHECK(withRtt.localInputDelayFrames() == 1);
        CHECK(withRtt.inputLeadFrames() + withRtt.localInputDelayFrames() == 19);  // ceil((300 + 2 * 150) / 33)
        CHECK(noRtt.inputLeadFrames() == high.m_minLeadFrames);

        // 关闭本地输入延迟时领先量自己覆盖整个 RTT。
        NetBattleSyncConfig off = high;
        off.m_localDelayMode    = LocalDelayMode::Off;
        NetBattleSync noDelay(0, 2, off);
        noDelay.start(buildWorld(setup), 1000, 1000);
        noDelay.onServerFeedback(slackSample(20, 1, 2000), 2300);
        for (uint64_t f = 2; f <= 800; f += 5)
            noDelay.onServerFeedback(slackSample(20, f), 2300);
        CHECK(noDelay.localInputDelayFrames() == 0);
        CHECK(noDelay.inputLeadFrames() == 19);
    }
}

TEST_CASE("NetBattleSync lead changes never skip or rewrite local input frames")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSyncConfig config;
    config.m_inputDelayFrames = 4;
    config.m_rollbackDepth    = 120;
    NetBattleSync sync(0, 2, config);
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);

    std::map<uint64_t, std::string> sent;
    uint64_t nextEdge = 0;
    // 与 NetBattleMode::onUpdate 一致的推进循环：只在 当前帧 < 目标帧 时前进一帧，每帧号只生成一次输入。
    auto runClock = [&](uint64_t fromMs, uint64_t toMs) {
        for (uint64_t now = fromMs; now <= toMs; now += 16)
        {
            const uint64_t target = sync.targetFrame(now);
            uint32_t catchUp      = 0;
            while (sync.session().currentFrame() < target && catchUp < 4 && sync.session().canAdvance())
            {
                const uint64_t frame = sync.session().currentFrame() + 1;
                PlayerInput in;
                in.slot = 0;
                if (frame % 3 == 0)
                    in = atkInput(0);
                sync.submitLocalInput(frame, in);
                REQUIRE(sync.session().tick());
                CHECK(sync.session().currentFrame() == frame);
                std::vector<std::pair<uint64_t, PlayerInput>> pending;
                sync.collectPendingInputs(pending);
                for (const auto& [f, input] : pending)
                {
                    const std::string key = std::to_string(input.commands.size());
                    const auto [it, inserted] = sent.emplace(f, key);
                    CHECK((inserted || it->second == key));
                }
                nextEdge = frame;
                ++catchUp;
            }
        }
    };

    runClock(1000, 1000 + 33 * 40);
    const uint64_t frameBefore = sync.session().currentFrame();
    CHECK(frameBefore >= 40);

    // 增大领先量：多追几帧，帧号连续不跳。
    sync.onServerFeedback(slackSample(-6, frameBefore + 1), 2000);
    const uint32_t raised = sync.inputLeadFrames();
    CHECK(raised > 4);
    runClock(2000, 2000 + 33 * 10);
    CHECK(sync.session().currentFrame() >= frameBefore + (raised - 4));

    // 缩小领先量：目标帧回落，客户端等待（不再前进、不重发不同内容），直到目标帧重新追上。
    const uint64_t frameHigh = sync.session().currentFrame();
    for (uint64_t f = frameHigh + 1; f <= frameHigh + 200; f += 5)
        sync.onServerFeedback(slackSample(30, f), 3000);
    CHECK(sync.inputLeadFrames() < raised);
    const uint64_t target = sync.targetFrame(3000 + 33 * 10);
    runClock(3000 + 33 * 10, 3000 + 33 * 10);
    CHECK(sync.session().currentFrame() >= frameHigh);
    CHECK((target > frameHigh || sync.session().currentFrame() == frameHigh));
    (void)nextEdge;
}

TEST_CASE("LockstepAuthority carries a late non-empty input into the next frame")
{
    test::ScenarioHarness::loadAssets();
    LockstepAuthority authority(duelSetup(2, kTestSeed));
    for (int i = 0; i < 5; ++i)
        authority.step();  // 已 tick 到帧 5。
    REQUIRE(authority.currentFrame() == 5);

    // 帧 3 的攻击迟到：顺延到帧 6，排在帧 6 自身命令（跑动）之前。
    CHECK(authority.needsInput(1, 3));
    CHECK(authority.submitInput(1, 3, atkInput(1)));
    CHECK_FALSE(authority.needsInput(1, 3));  // 已收到过，重发被跳过。
    CHECK(authority.submitInput(1, 3, atkInput(1)));  // 重复提交视为已处理，不重复顺延。
    CHECK(authority.submitInput(1, 6, runInput(1)));

    const auto& stats = authority.inputStats(1);
    CHECK(stats.m_lateInputs == 1);
    CHECK(stats.m_carriedInputs == 1);
    CHECK(stats.m_droppedInputs == 0);
    CHECK(stats.m_windowSamples == 2);
    CHECK(stats.m_windowMinSlack == -3);  // 3 - 5 - 1
    CHECK(stats.m_windowMinFrame == 3);

    const auto f6 = authority.step();
    REQUIRE(f6.players[1].commands.size() == 2);
    CHECK(f6.players[1].commands[0].controlEvent == ControlEvent::CTR_ATKA);
    CHECK(f6.players[1].commands[1].controlEvent == ControlEvent::CTR_RUN);
    // 顺延只发生一次。
    const auto f7 = authority.step();
    CHECK(f7.players[1].commands.empty());

    // 槽位 0 完全不受影响。
    CHECK(authority.inputStats(0).m_lateInputs == 0);
}

TEST_CASE("LockstepAuthority drops stale late input, counts empty late input and tracks slack")
{
    test::ScenarioHarness::loadAssets();
    LockstepAuthority authority(duelSetup(2, kTestSeed));
    for (int i = 0; i < 40; ++i)
        authority.step();

    // 迟到 kMaxLateCarryFrames 帧及以上：丢弃。
    CHECK_FALSE(authority.submitInput(0, 40 - kMaxLateCarryFrames, atkInput(0)));
    CHECK(authority.inputStats(0).m_droppedInputs == 1);
    CHECK(authority.inputStats(0).m_carriedInputs == 0);
    // 迟到但仍在上限内：顺延。
    CHECK(authority.submitInput(0, 40 - kMaxLateCarryFrames + 1, atkInput(0)));
    CHECK(authority.inputStats(0).m_carriedInputs == 1);
    // 空输入迟到只计数，不顺延也不算丢弃。
    CHECK(authority.submitInput(0, 39, PlayerInput{}));
    CHECK(authority.inputStats(0).m_lateInputs == 3);
    CHECK(authority.inputStats(0).m_droppedInputs == 1);
    CHECK(authority.inputStats(0).m_carriedInputs == 1);

    // 准时输入：富余 = 帧号 - 已 tick 帧 - 1；窗口记最小值，重置后清零而累计计数保留。
    CHECK(authority.submitInput(1, 43, atkInput(1)));  // 富余 2
    CHECK(authority.submitInput(1, 41, atkInput(1)));  // 富余 0
    CHECK(authority.inputStats(1).m_windowMinSlack == 0);
    CHECK(authority.inputStats(1).m_windowMinFrame == 41);
    CHECK(authority.inputStats(1).m_windowSamples == 2);
    authority.resetSlackWindow(1);
    CHECK(authority.inputStats(1).m_windowSamples == 0);
    CHECK(authority.inputStats(1).m_lateInputs == 0);

    // releaseSlot 清掉顺延与已收记录：同一帧号重新被接受（重连后的新客户端）。
    authority.releaseSlot(0);
    CHECK(authority.needsInput(0, 41));
}

TEST_CASE("LockstepAuthority keeps the last move state of a stale input instead of dropping it")
{
    test::ScenarioHarness::loadAssets();
    LockstepAuthority authority(duelSetup(2, kTestSeed));
    for (int i = 0; i < 40; ++i)
        authority.step();

    // 迟到超过上限的输入：攻击被丢弃，但跑动 / 待机是电平语义，只保留最后一条顺延，角色不会永久停在旧状态。
    PlayerInput stale;
    stale.slot = 0;
    stale.commands.push_back({ControlEvent::CTR_ATKA, ControlQuadrant::None, 0, 1});
    stale.commands.push_back({ControlEvent::CTR_RUN, ControlQuadrant::UpRight, 0, 1});
    stale.commands.push_back({ControlEvent::CTR_IDLE, ControlQuadrant::None, 0, 0});
    CHECK(authority.submitInput(0, 40 - kMaxLateCarryFrames, stale));
    CHECK(authority.inputStats(0).m_carriedInputs == 1);
    CHECK(authority.inputStats(0).m_droppedInputs == 0);
    const auto frame = authority.step();
    REQUIRE(frame.players[0].commands.size() == 1);
    CHECK(frame.players[0].commands[0].controlEvent == ControlEvent::CTR_IDLE);
}

TEST_CASE("NetBattleSync calibrates the lead from the first RTT sample and gates local input until then")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSyncConfig config;
    config.m_inputDelayFrames = 4;
    NetBattleSync sync(0, 2, config);
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);

    // 校准前不采样本地输入；超时后放行。
    CHECK_FALSE(sync.inputReady(1000, 5));
    CHECK_FALSE(sync.inputReady(1000 + config.m_calibrationTimeoutMs - 1, 5));
    CHECK(sync.inputReady(1000 + config.m_calibrationTimeoutMs, 5));

    // 首个 RTT 样本 400ms：领先量直接抬到覆盖 RTT（含四分之一余量与目标富余），而不是等迟到反馈逐轮补。
    sync.onServerFeedback(slackSample(5, 1, /*echo*/ 2000), 2400);
    CHECK(sync.rttMs() == 400);
    CHECK(sync.inputLeadFrames() >= (400 + 100) / 33);
    // 追帧中的帧（早已落后于估计服务器帧 + 领先量）不采样输入；追到位的帧才放行。
    const uint64_t serverFrame = sync.estimatedServerFrame(2400);
    CHECK_FALSE(sync.inputReady(2400, serverFrame + 1));
    CHECK(sync.inputReady(2400, serverFrame + sync.inputLeadFrames()));
}

TEST_CASE("PoseTrack turns a rollback position rewrite into a decaying offset instead of a snap")
{
    mugen::PoseTrack track;
    track.sample(10, {100.0f, 0.0f, 50.0f});
    track.sample(11, {110.0f, 0.0f, 50.0f});
    const float before = track.display(1.0f).x;
    CHECK(before == doctest::Approx(110.0f));

    // 回滚把当前帧位置往回改写 40 单位：显示位置在应用纠正的那一刻不变。
    track.applyCorrection({-40.0f, 0.0f, 0.0f});
    CHECK(track.display(1.0f).x == doctest::Approx(before));
    CHECK(track.offsetLength() == doctest::Approx(40.0f));

    // 之后随渲染时间平滑衰减到模拟位置，不会反向跳。
    float last = track.display(1.0f).x;
    for (int i = 0; i < 60; ++i)
    {
        track.advance(1.0f / 60.0f);
        const float now = track.display(1.0f).x;
        CHECK(now <= last + 0.001f);
        last = now;
    }
    CHECK(last == doctest::Approx(70.0f).epsilon(0.02));

    // 跳帧过大（重同步）直接吸附并清掉偏移；追帧几帧仍平滑；回退帧号同样吸附。
    track.applyCorrection({-30.0f, 0.0f, 0.0f});
    track.sample(11 + mugen::PoseTrack::kMaxInterpolatedGap + 5, {500.0f, 0.0f, 50.0f});
    CHECK(track.offsetLength() == 0.0f);
    CHECK(track.display(0.0f).x == doctest::Approx(500.0f));

    // tick 进度从 tick 起算且停在 1。
    mugen::TickAlpha alpha;
    alpha.onRenderFrame(0.016f, true);
    CHECK(alpha.alpha(0.033f) == 0.0f);
    alpha.onRenderFrame(0.016f, false);
    CHECK(alpha.alpha(0.033f) == doctest::Approx(0.016f / 0.033f));
    for (int i = 0; i < 10; ++i)
        alpha.onRenderFrame(0.016f, false);
    CHECK(alpha.alpha(0.033f) == 1.0f);
}

TEST_CASE("PoseTrack remote profile glides large corrections and always decays to zero")
{
    using mugen::CorrectionProfile;
    using mugen::PoseTrack;

    // 本地档：超过 kMaxCorrection 的纠正直接吸附（偏移清零）；远端档同样大小仍然滑行。
    {
        PoseTrack local;
        local.sample(10, {100.0f, 0.0f, 0.0f});
        local.sample(11, {110.0f, 0.0f, 0.0f});
        local.applyCorrection({-450.0f, 0.0f, 0.0f});
        CHECK(local.offsetLength() == 0.0f);

        PoseTrack remote;
        remote.setProfile(CorrectionProfile::Remote);
        remote.sample(10, {100.0f, 0.0f, 0.0f});
        remote.sample(11, {110.0f, 0.0f, 0.0f});
        const float before = remote.display(1.0f).x;
        remote.applyCorrection({-450.0f, 0.0f, 0.0f});
        CHECK(remote.offsetLength() == doctest::Approx(450.0f));
        CHECK(remote.display(1.0f).x == doctest::Approx(before));  // 应用纠正的那一刻显示位置不变。
    }
    // 远端档的吸附阈值：900 以内滑行，超过才吸附。
    {
        PoseTrack remote;
        remote.setProfile(CorrectionProfile::Remote);
        remote.sample(10, {0.0f, 0.0f, 0.0f});
        remote.sample(11, {10.0f, 0.0f, 0.0f});
        remote.applyCorrection({-PoseTrack::kRemoteMaxCorrection + 1.0f, 0.0f, 0.0f});
        CHECK(remote.offsetLength() > 0.0f);
        remote.applyCorrection({-5.0f, 0.0f, 0.0f});
        CHECK(remote.offsetLength() == 0.0f);
    }
    // 时间常数随偏移增大：偏移大时更长，偏移衰减后回落到最小值。
    {
        PoseTrack small;
        PoseTrack large;
        small.setProfile(CorrectionProfile::Remote);
        large.setProfile(CorrectionProfile::Remote);
        small.sample(1, {0.0f, 0.0f, 0.0f});
        large.sample(1, {0.0f, 0.0f, 0.0f});
        small.applyCorrection({-20.0f, 0.0f, 0.0f});
        large.applyCorrection({-400.0f, 0.0f, 0.0f});
        CHECK(small.correctionTauSec() < large.correctionTauSec());
        CHECK(large.correctionTauSec() <= PoseTrack::kRemoteCorrectionTauMaxSec + 1e-6f);
        CHECK(small.correctionTauSec() >= PoseTrack::kRemoteCorrectionTauMinSec - 1e-6f);
        // 远端 400 单位的纠正比本地档同量级的 0.1s 常数慢得多：0.1s 后本地档只剩约 37%，远端档剩得更多。
        PoseTrack localSmall;
        localSmall.sample(1, {0.0f, 0.0f, 0.0f});
        localSmall.applyCorrection({-300.0f, 0.0f, 0.0f});
        PoseTrack remote300;
        remote300.setProfile(CorrectionProfile::Remote);
        remote300.sample(1, {0.0f, 0.0f, 0.0f});
        remote300.applyCorrection({-300.0f, 0.0f, 0.0f});
        for (int i = 0; i < 6; ++i)
        {
            localSmall.advance(1.0f / 60.0f);
            remote300.advance(1.0f / 60.0f);
        }
        CHECK(remote300.offsetLength() > localSmall.offsetLength());
    }
    // 偏移单调衰减且最终严格归零（远端静止时不留下漂移）。
    {
        PoseTrack remote;
        remote.setProfile(CorrectionProfile::Remote);
        remote.sample(1, {50.0f, 0.0f, 20.0f});
        remote.applyCorrection({-380.0f, 0.0f, 15.0f});
        float last = remote.offsetLength();
        int frames = 0;
        while (remote.offsetLength() > 0.0f && frames < 3000)
        {
            remote.advance(1.0f / 60.0f);
            CHECK(remote.offsetLength() <= last + 1e-4f);
            last = remote.offsetLength();
            ++frames;
        }
        CHECK(remote.offsetLength() == 0.0f);
        CHECK(frames < 600);  // 10 秒内必然归零（实际约 2 秒）。
        CHECK(remote.display(1.0f).x == doctest::Approx(50.0f - 380.0f));
    }
    // 重同步（跳帧 / 帧号回退）在远端档同样直接吸附并清掉偏移。
    {
        PoseTrack remote;
        remote.setProfile(CorrectionProfile::Remote);
        remote.sample(20, {0.0f, 0.0f, 0.0f});
        remote.sample(21, {5.0f, 0.0f, 0.0f});
        remote.applyCorrection({-200.0f, 0.0f, 0.0f});
        remote.sample(15, {0.0f, 0.0f, 0.0f});
        CHECK(remote.offsetLength() == 0.0f);
    }
}

namespace
{
// 一条不含 RTT 的空反馈（仅用于驱动本地输入延迟的调整）。
ServerInputFeedback emptyFeedback()
{
    return {};
}

// 喂入一个 RTT 样本：回显时间 = now - rtt。
void feedRtt(NetBattleSync& sync, uint64_t now, int64_t rtt, uint64_t slackFrame = 1)
{
    sync.onServerFeedback(slackSample(30, slackFrame, now - static_cast<uint64_t>(rtt)), now);
}
}  // namespace

TEST_CASE("NetBattleSync local input delay: threshold, ramp, cap")
{
    test::ScenarioHarness::loadAssets();
    const auto setup = duelSetup(2, kTestSeed);

    // 不超过阈值（250ms）为 0；超过 1ms 即 1 帧。
    {
        NetBattleSync sync(0, 2, {});
        sync.start(buildWorld(setup), 1000, 1000);
        feedRtt(sync, 5000, 250);
        CHECK(sync.localInputDelayFrames() == 0);
    }
    {
        NetBattleSync sync(0, 2, {});
        sync.start(buildWorld(setup), 1000, 1000);
        feedRtt(sync, 5000, 251);
        CHECK(sync.localInputDelayFrames() == 1);
    }
    // 400ms：目标 ceil((400 - 250) / 2 / 33) = 3，每 100ms 升 1 帧（升得快、一次只升 1 帧）。
    {
        NetBattleSync sync(0, 2, {});
        sync.start(buildWorld(setup), 1000, 1000);
        feedRtt(sync, 5000, 400);
        CHECK(sync.localInputDelayFrames() == 1);
        feedRtt(sync, 5050, 400);
        CHECK(sync.localInputDelayFrames() == 1);  // 间隔不足。
        feedRtt(sync, 5100, 400);
        CHECK(sync.localInputDelayFrames() == 2);
        feedRtt(sync, 5200, 400);
        CHECK(sync.localInputDelayFrames() == 3);
        feedRtt(sync, 5300, 400);
        feedRtt(sync, 5400, 400);
        CHECK(sync.localInputDelayFrames() == 3);
        // 领先量 + D 守恒：首个样本把领先量抬到覆盖 RTT，之后 D 每升 1 帧领先量降 1 帧。
        CHECK(sync.inputLeadFrames() + sync.localInputDelayFrames() == (400 + 100 + 32) / 33 + 2);
    }
    // 极高 RTT 受上限封顶。
    {
        NetBattleSyncConfig config;
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        for (uint64_t i = 0; i < 20; ++i)
            feedRtt(sync, 5000 + i * 100, 1500, 1 + i);
        CHECK(sync.localInputDelayFrames() == config.m_localDelayMaxFrames);
    }
}

TEST_CASE("NetBattleSync local input delay lowers slowly with hysteresis, one frame at a time")
{
    test::ScenarioHarness::loadAssets();
    const auto setup = duelSetup(2, kTestSeed);
    NetBattleSyncConfig config;

    // RTT 在阈值之下 30ms 的滞回带内：不降。
    {
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        feedRtt(sync, 5000, 270);
        CHECK(sync.localInputDelayFrames() == 1);
        uint64_t now = 5000;
        for (int i = 0; i < 400; ++i)
        {
            now += 33;
            feedRtt(sync, now, 245, 2 + i);  // 平滑 RTT 收敛到 245：低于阈值，但 245 + 30 仍高于阈值。
        }
        CHECK(sync.localInputDelayFrames() == 1);
        // RTT 真正下降到滞回带之外，持续 2 秒后才降到 0。
        const uint64_t lowStart = now;
        uint64_t loweredAt      = 0;
        for (int i = 0; i < 600 && loweredAt == 0; ++i)
        {
            now += 33;
            feedRtt(sync, now, 150, 1000 + i);
            if (sync.localInputDelayFrames() == 0)
                loweredAt = now;
        }
        CHECK(loweredAt != 0);
        CHECK(loweredAt - lowStart >= static_cast<uint64_t>(config.m_localDelayLowerHoldMs));
    }
    // 从 D = 3 逐帧下降：任意相邻两次反馈之间 D 至多变化 1，每次下降间隔不小于保持时长，最终稳定在 1。
    {
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        uint64_t now = 5000;
        for (int i = 0; i < 10; ++i)
        {
            feedRtt(sync, now, 400, 1 + i);
            now += 100;
        }
        CHECK(sync.localInputDelayFrames() == 3);
        uint32_t last        = 3;
        uint64_t lastDropAt  = 0;
        for (int i = 0; i < 1000; ++i)
        {
            now += 33;
            feedRtt(sync, now, 270, 100 + i);
            const uint32_t d = sync.localInputDelayFrames();
            CHECK(d <= last);
            CHECK(last - d <= 1);
            if (d < last && lastDropAt != 0)
                CHECK(now - lastDropAt >= static_cast<uint64_t>(config.m_localDelayLowerHoldMs));
            if (d < last)
                lastDropAt = now;
            last = d;
        }
        CHECK(last == 1);
    }
    // 强制 / 关闭模式：D 逐帧趋近目标。
    {
        NetBattleSync sync(0, 2, config);
        sync.start(buildWorld(setup), 1000, 1000);
        sync.setLocalDelayMode(LocalDelayMode::Forced, 4);
        uint64_t now = 2000;
        uint32_t last = 0;
        for (int i = 0; i < 12; ++i)
        {
            sync.onServerFeedback(emptyFeedback(), now);
            CHECK(sync.localInputDelayFrames() - last <= 1);
            last = sync.localInputDelayFrames();
            now += 100;
        }
        CHECK(sync.localInputDelayFrames() == 4);
        sync.setLocalDelayMode(LocalDelayMode::Off, 0);
        for (int i = 0; i < 12; ++i)
        {
            sync.onServerFeedback(emptyFeedback(), now);
            now += 100;
        }
        CHECK(sync.localInputDelayFrames() == 0);
    }
}

TEST_CASE("NetBattleSync local input delay never drops, duplicates or rewrites a local input frame")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSyncConfig config;
    config.m_rollbackDepth = 120;
    NetBattleSync sync(0, 2, config);
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);
    // 100ms RTT 校准后放行输入采样，D 由强制模式脚本驱动。
    sync.onServerFeedback(slackSample(20, 1, 1900), 2000);
    REQUIRE(sync.rttMs() == 100);

    // 与 NetBattleMode::onUpdate 一致的推进循环：每个采样都带唯一编号（放在 angle 字段），
    // 被采样的编号必须在发出的输入里恰好出现一次。
    std::map<uint64_t, PlayerInput> sent;
    std::vector<int32_t> sampledIds;
    std::map<int32_t, int> idCount;
    int32_t nextId     = 1;
    uint32_t skipTicks = 0;
    std::vector<uint32_t> delayTrace;
    uint32_t lastDelay = sync.localInputDelayFrames();
    auto drain = [&]() {
        std::vector<std::pair<uint64_t, PlayerInput>> pending;
        sync.collectPendingInputs(pending);
        for (const auto& [f, input] : pending)
        {
            const auto [it, inserted] = sent.emplace(f, input);
            if (!inserted)
            {
                // 冗余重发必须与首次发出逐字段一致。
                REQUIRE(it->second.commands.size() == input.commands.size());
                for (size_t i = 0; i < input.commands.size(); ++i)
                    CHECK(it->second.commands[i].angle == input.commands[i].angle);
            }
        }
    };
    uint64_t nextFeedbackAt = 0;
    auto runClock = [&](uint64_t fromMs, uint64_t toMs) {
        for (uint64_t now = fromMs; now <= toMs; now += 16)
        {
            if (now >= nextFeedbackAt)
            {
                nextFeedbackAt = now + 96;
                sync.onServerFeedback(emptyFeedback(), now);
                if (sync.localInputDelayFrames() != lastDelay)
                {
                    CHECK((sync.localInputDelayFrames() > lastDelay ? sync.localInputDelayFrames() - lastDelay
                                                                    : lastDelay - sync.localInputDelayFrames()) == 1);
                    lastDelay = sync.localInputDelayFrames();
                    delayTrace.push_back(lastDelay);
                }
            }
            const uint64_t target = sync.targetFrame(now);
            uint32_t catchUp      = 0;
            while (sync.session().currentFrame() < target && catchUp < 4 && sync.session().canAdvance())
            {
                const uint64_t frame = sync.session().currentFrame() + 1;
                PlayerInput in;
                in.slot = 0;
                if (sync.inputReady(now, frame))
                {
                    PlayerCommand command{ControlEvent::CTR_RUN, ControlQuadrant::UpRight, nextId, 1};
                    in.commands.push_back(command);
                    sampledIds.push_back(nextId);
                    ++nextId;
                }
                else
                    ++skipTicks;
                sync.submitLocalInput(frame, in);
                REQUIRE(sync.session().tick());
                drain();
                ++catchUp;
            }
        }
    };

    // 强制 D：0 -> 3（缓冲）-> 1（缩短，帧间不重不丢）-> 4 -> 0。
    uint64_t t = 2000;
    runClock(t, t + 800);
    sync.setLocalDelayMode(LocalDelayMode::Forced, 3);
    t += 800;
    runClock(t, t + 1500);
    CHECK(sync.localInputDelayFrames() == 3);
    sync.setLocalDelayMode(LocalDelayMode::Forced, 1);
    t += 1500;
    runClock(t, t + 1500);
    CHECK(sync.localInputDelayFrames() == 1);
    sync.setLocalDelayMode(LocalDelayMode::Forced, 4);
    t += 1500;
    runClock(t, t + 1500);
    CHECK(sync.localInputDelayFrames() == 4);
    sync.setLocalDelayMode(LocalDelayMode::Off, 0);
    t += 1500;
    runClock(t, t + 1500);
    CHECK(sync.localInputDelayFrames() == 0);
    CHECK(skipTicks > 0);  // D 减小时确实跳过了采样（而不是改写已安排的帧）。

    // D 的轨迹按 1 帧逐步变化。
    const std::vector<uint32_t> expectedTrace = {1, 2, 3, 2, 1, 2, 3, 4, 3, 2, 1, 0};
    CHECK(delayTrace == expectedTrace);

    // 发出的输入帧号连续无空洞，每个采样编号恰好出现一次，且按帧序递增。
    const uint64_t scheduled = sync.scheduledThroughFrame();
    REQUIRE(scheduled > 100);
    for (uint64_t f = 1; f <= scheduled; ++f)
        CHECK(sent.contains(f));
    int32_t lastSeen = 0;
    for (const auto& [f, input] : sent)
    {
        for (const auto& command : input.commands)
        {
            ++idCount[command.angle];
            CHECK(command.angle > lastSeen);
            lastSeen = command.angle;
        }
    }
    for (const int32_t id : sampledIds)
        CHECK(idCount[id] == 1);
    CHECK(idCount.size() == sampledIds.size());

    // 本地英雄不被错误预测：服务器按发出的输入确认后，本地槽位没有任何错误预测与回滚。
    const uint64_t current = sync.session().currentFrame();
    for (uint64_t f = 1; f <= current; ++f)
    {
        sync.confirmInput(f, 0, sent.at(f));
        sync.confirmInput(f, 1, PlayerInput{});
    }
    sync.flushRollback();
    CHECK(sync.session().stats().localMispredictions == 0);
    // 远端槽位这里确认的恰好是空输入，没有任何改写。
    CHECK(sync.session().stats().rollbacks == 0);
}

TEST_CASE("NetBattleSync local input delay keeps lead plus delay and gates one tick when it shrinks")
{
    test::ScenarioHarness::loadAssets();
    NetBattleSyncConfig config;
    NetBattleSync sync(0, 2, config);
    sync.start(buildWorld(duelSetup(2, kTestSeed)), 1000, 1000);
    sync.onServerFeedback(slackSample(20, 1, 1840), 2000);  // RTT 160。
    REQUIRE(sync.rttMs() == 160);
    const uint32_t leadBase = sync.inputLeadFrames();

    sync.setLocalDelayMode(LocalDelayMode::Forced, 3);
    uint64_t now = 2000;
    for (int i = 0; i < 4; ++i)
    {
        sync.onServerFeedback(emptyFeedback(), now);
        now += 100;
    }
    CHECK(sync.localInputDelayFrames() == 3);
    CHECK(sync.inputLeadFrames() + 3 == leadBase);

    // 推进到稳态（按真实循环：放行才采样，每帧提交一次），时钟停在估计服务器帧 60 附近。
    now = 1000 + 33 * 60;
    while (sync.session().currentFrame() < sync.targetFrame(now))
    {
        const uint64_t f = sync.session().currentFrame() + 1;
        PlayerInput in;
        in.slot = 0;
        if (sync.inputReady(now, f))
            in = atkInput(0);
        sync.submitLocalInput(f, in);
        REQUIRE(sync.session().tick());
    }
    const uint64_t frame = sync.session().currentFrame() + 1;
    // 稳态：输入帧领先推进帧恰好 D。
    CHECK(sync.scheduledThroughFrame() == sync.session().currentFrame() + 3);
    CHECK(sync.inputReady(now, frame));

    // D 从 3 降到 2（领先量同时 +1）：本帧的生效帧 frame + 2 已被占用，不放行也不改写；下一帧恢复放行。
    sync.setLocalDelayMode(LocalDelayMode::Forced, 2);
    sync.onServerFeedback(emptyFeedback(), now + 200);
    CHECK(sync.localInputDelayFrames() == 2);
    CHECK(sync.inputLeadFrames() + 2 == leadBase);
    CHECK_FALSE(sync.inputReady(now, frame));
    sync.submitLocalInput(frame, PlayerInput{});
    CHECK(sync.scheduledThroughFrame() == frame + 2);  // 没有新安排，也没有改写。
    CHECK(sync.inputReady(now, frame + 1));
}
