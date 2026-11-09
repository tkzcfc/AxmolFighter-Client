#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/sim_legacy/BattleBootstrap.h"
#include "mugen/sim_legacy/net/LockstepAuthority.h"
#include "mugen/sim_legacy/net/NetCodec.h"
#include "mugen/sim_legacy/rollback/RollbackSession.h"
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace mugen;
using namespace mugen::sim_legacy;
using namespace mugen::sim_legacy::net;
using namespace mugen::sim_legacy::rollback;

namespace
{
constexpr int32_t kDuelRoom  = 30002;  // 纯 PvP 竞技场，无怪物、无警告。
constexpr uint64_t kTestSeed = 20261;

// 单帧全部槽位的脚本输入。
using FrameScript = std::array<PlayerInput, kMaxBattlePlayers>;

// 生成确定性、可复现的逐帧逐槽位输入脚本；每帧每槽位都产生一条命令。
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
        p.m_slot    = static_cast<uint32_t>(slot);
        p.m_class   = CharacterClass::kSwordman;
        p.m_roleId  = 101;
        p.m_level   = 100;
        p.m_camp    = slot == 0 ? 2 : 8;
        p.m_isAi    = false;
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

// 运行权威世界并按延迟投递输入，返回每帧确认输入；可选在指定帧捕获快照。
std::vector<FrameInput> runAuthority(LockstepAuthority& authority,
                                     const std::vector<FrameScript>& script,
                                     int playerCount,
                                     uint64_t frames,
                                     uint64_t snapshotFrame = 0,
                                     std::string* snapshot   = nullptr)
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
        if (snapshot && frame == snapshotFrame)
            authority.serializeWorld(*snapshot);
    }
    return confirmed;
}

// 驱动单个客户端：以广播延迟消费确认输入，并领先预测本地槽位。
void driveClient(RollbackSession& session,
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
                session.addConfirmedInput(f, slot, confirmed[f].players[slot]);
        const uint64_t target = std::min<uint64_t>(step + lead, frames);
        if (step + lead <= frames)
            session.addLocalInput(target, script[target - 1][localSlot]);
        session.advanceTo(target);
    }
    // 冲洗末尾可能残留的回滚，确保最终确认帧哈希稳定。
    if (session.currentFrame() == frames)
        session.tick();
}

bool sameInput(const PlayerInput& a, const PlayerInput& b)
{
    if (a.slot != b.slot || a.commands.size() != b.commands.size())
        return false;
    for (size_t i = 0; i < a.commands.size(); ++i)
    {
        const auto& ca = a.commands[i];
        const auto& cb = b.commands[i];
        if (ca.controlEvent != cb.controlEvent || ca.quadrant != cb.quadrant || ca.angle != cb.angle ||
            ca.pressType != cb.pressType)
            return false;
    }
    return true;
}
}  // namespace

TEST_CASE("Net codec round-trips setup, input and frame")
{
    const auto setup      = duelSetup(2, kTestSeed);
    const std::string set = encodeWorldSetup(setup);
    WorldSetup decoded;
    REQUIRE(decodeWorldSetup(set, decoded));
    CHECK(decoded.m_players.size() == 2);
    CHECK(decoded.m_players[0].m_slot == 0);
    CHECK(decoded.m_players[0].m_class == CharacterClass::kSwordman);
    CHECK(decoded.m_players[1].m_camp == 8);
    CHECK(decoded.m_pvp);
    CHECK(encodeWorldSetup(decoded) == set);

    PlayerInput in;
    in.slot = 1;
    PlayerCommand command;
    command.controlEvent = ControlEvent::CTR_ATKA;
    command.quadrant     = ControlQuadrant::UpRight;
    command.angle        = 45000;
    command.pressType    = 1;
    in.commands.push_back(command);
    const std::string pin = encodePlayerInput(in);
    PlayerInput pout;
    REQUIRE(decodePlayerInput(pin, pout));
    CHECK(pout.commands.size() == 1);
    CHECK(pout.commands[0].controlEvent == ControlEvent::CTR_ATKA);
    CHECK(encodePlayerInput(pout) == pin);

    FrameInput frame;
    frame.frame = 7;
    frame.players.push_back(in);
    frame.systemCommands.push_back(PauseCommand{true});
    const std::string fbytes = encodeFrameInput(frame);
    FrameInput fout;
    REQUIRE(decodeFrameInput(fbytes, fout));
    CHECK(fout.frame == 7);
    CHECK(fout.players.size() == 1);
    CHECK(fout.systemCommands.size() == 1);
    CHECK(encodeFrameInput(fout) == fbytes);
}

TEST_CASE("Lockstep authority rejects late, far and out-of-range inputs")
{
    test::ScenarioHarness::loadAssets();
    LockstepAuthority authority(duelSetup(2, kTestSeed));

    PlayerInput in;
    in.slot = 0;
    in.commands.push_back({ControlEvent::CTR_ATKA, ControlQuadrant::None, 0, 1});

    CHECK(authority.submitInput(0, 1, in));                     // 合法下一帧
    CHECK(authority.submitInput(0, kMaxInputLead, in));         // 合法上界
    CHECK_FALSE(authority.submitInput(0, kMaxInputLead + 1, in));  // 超前过多
    CHECK_FALSE(authority.submitInput(0, 0, in));               // 迟到（已确认帧）
    CHECK_FALSE(authority.submitInput(2, 1, in));               // 槽位越界
}

TEST_CASE("Lockstep loopback 2-player matches authority hashes")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 300;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    CHECK(authority.slotCount() == playerCount);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);

    // 验证确实同时发生了准时确认与迟到替换。
    bool sawInTime       = false;
    bool sawSubstitution = false;
    for (uint64_t f = 1; f <= frames; ++f)
        for (uint32_t s = 0; s < playerCount; ++s)
        {
            if (sameInput(confirmed[f].players[s], script[f - 1][s]))
                sawInTime = true;
            else
                sawSubstitution = true;
        }
    CHECK(sawInTime);
    CHECK(sawSubstitution);

    RollbackSession clientA(0, playerCount, static_cast<uint32_t>(frames + 64));
    RollbackSession clientB(1, playerCount, static_cast<uint32_t>(frames + 64));
    clientA.setWorld(buildWorld(setup));
    clientB.setWorld(buildWorld(setup));
    driveClient(clientA, 0, script, confirmed, playerCount, frames, 2, 5);
    driveClient(clientB, 1, script, confirmed, playerCount, frames, 2, 5);

    CHECK(clientA.stats().rollbacks > 0);
    CHECK(clientB.stats().rollbacks > 0);

    const uint64_t commonConfirmed = std::min(clientA.confirmedFrame(), clientB.confirmedFrame());
    CHECK(commonConfirmed >= frames);
    for (uint64_t h = kHashInterval; h <= commonConfirmed; h += kHashInterval)
    {
        INFO("hash frame " << h);
        CHECK(clientA.hashAt(h) == authority.hashAt(h));
        CHECK(clientB.hashAt(h) == authority.hashAt(h));
    }
}

TEST_CASE("Lockstep loopback 4-player matches authority hashes")
{
    constexpr int playerCount = 4;
    constexpr uint64_t frames = 120;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);

    std::vector<RollbackSession> clients;
    clients.reserve(playerCount);
    for (uint32_t slot = 0; slot < playerCount; ++slot)
    {
        RollbackSession client(slot, playerCount, static_cast<uint32_t>(frames + 64));
        client.setWorld(buildWorld(setup));
        driveClient(client, slot, script, confirmed, playerCount, frames, 2, 5);
        clients.push_back(std::move(client));
    }

    uint64_t commonConfirmed = std::numeric_limits<uint64_t>::max();
    for (const auto& client : clients)
        commonConfirmed = std::min(commonConfirmed, client.confirmedFrame());
    CHECK(commonConfirmed >= frames);
    for (uint64_t h = kHashInterval; h <= commonConfirmed; h += kHashInterval)
        for (const auto& client : clients)
        {
            INFO("hash frame " << h);
            CHECK(client.hashAt(h) == authority.hashAt(h));
        }
}

TEST_CASE("Lockstep authority reports PVP timeout outcome")
{
    test::ScenarioHarness::loadAssets();
    auto setup         = duelSetup(2, kTestSeed);
    setup.m_pvpTimeLimit = Fixed::fromInt(1);  // 约三十帧后触发超时判定。
    LockstepAuthority authority(setup);
    for (int i = 0; i < 60; ++i)
        authority.step();
    const auto out = authority.outcome();
    REQUIRE(out.has_value());
    CHECK(out->m_reason == PvpRoundEndReason::Timeout);
}

TEST_CASE("Lockstep authority snapshot resync recovers a desynced client")
{
    constexpr int playerCount = 2;
    constexpr uint64_t frames = 200;
    test::ScenarioHarness::loadAssets();
    const auto setup  = duelSetup(playerCount, kTestSeed);
    const auto script = makeScript(frames, playerCount, kTestSeed);

    LockstepAuthority authority(setup);
    const auto confirmed = runAuthority(authority, script, playerCount, frames);

    RollbackSession client(0, playerCount);
    client.setWorld(buildWorld(setup));
    driveClient(client, 0, script, confirmed, playerCount, frames, 2, 5);
    CHECK(client.hashAt(frames) == authority.hashAt(frames));

    // 制造失步：篡改角色位置。
    client.world().ecs().each<Transform>([](Entity, Transform& t) { t.position.x = t.position.x + Fixed::fromInt(1); });
    CHECK(client.hash() != authority.hashAt(frames));

    // 采用权威当前快照复位到最终帧。
    std::string snapshot;
    authority.serializeWorld(snapshot);
    ByteBuffer buffer;
    for (uint8_t b : snapshot)
        buffer.writeByte(b);
    buffer.writeFinish();
    CHECK(client.resync(frames, buffer));
    CHECK(client.currentFrame() == frames);
    CHECK(client.hash() == authority.hashAt(frames));
}
