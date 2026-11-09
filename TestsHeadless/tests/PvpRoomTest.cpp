#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/sim_legacy/BattleBootstrap.h"
#include "mugen/sim_legacy/room/RoomDirector.h"
#include "mugen/sim_legacy/spawn/RoleSpawn.h"

using namespace mugen;
using namespace mugen::sim_legacy;

namespace
{
// PVP 1v1 决斗房间测试夹具：两个分属敌对阵营的玩家英雄。
struct PvpFixture
{
    World m_world;
    Entity m_hero, m_enemy;
    PvpFixture()
    {
        test::ScenarioHarness::loadAssets();
        WorldHooks hooks;
        hooks.update.fill(WorldHooks::noEntityUpdate);
        hooks.collision = WorldHooks::noWorldUpdate;
        m_world         = World(hooks);
        WorldSetup setup;
        setup.m_pvp = true;
        m_world.init(setup);
        m_hero = m_world.create(GroupKind::Role);
        RoleIdentity own;
        own.roleType = 1;
        own.camp     = 2;
        own.playerSlot = 0;
        m_world.ecs().add(m_hero, own);
        m_world.ecs().add(m_hero, Transform{});
        m_world.ecs().add(m_hero, RoleStatus{});
        Vitals heroVitals;
        heroVitals.hp = heroVitals.maxHp = "100"_fx;
        m_world.ecs().add(m_hero, heroVitals);
        m_world.ecs().singleton<PlayerSlots>().entities[0] = m_hero;
        m_enemy = m_world.create(GroupKind::Role);
        RoleIdentity hostile;
        hostile.roleType = 1;
        hostile.camp     = 1;
        hostile.playerSlot = 1;
        m_world.ecs().add(m_enemy, hostile);
        m_world.ecs().add(m_enemy, Transform{});
        m_world.ecs().add(m_enemy, RoleStatus{});
        Vitals enemyVitals;
        enemyVitals.hp = enemyVitals.maxHp = "100"_fx;
        m_world.ecs().add(m_enemy, enemyVitals);
        m_world.ecs().singleton<PlayerSlots>().entities[1] = m_enemy;
        conf::RoomConfig config;
        config.id              = 30002;
        config.roomType        = 2;
        config.battleRuleType  = 0;
        config.monsterIds      = {-1};
        REQUIRE(room::RoomDirector(m_world).enter(config, 0, 0));
        ticks(6);
        REQUIRE(state().m_state == mugen::RoomState::Process);
    }
    sim_legacy::RoomState& state() { return m_world.ecs().singleton<sim_legacy::RoomState>(); }
    void ticks(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            FrameInput input;
            input.frame = m_world.ecs().singleton<WorldClock>().frame + 1;
            m_world.tick(input);
        }
    }
    template <class Event>
    const Event* lastEvent() const
    {
        const Event* found = nullptr;
        for (const auto& event : m_world.ecs().singleton<PresentationEvents>().events)
            if (const auto* payload = std::get_if<Event>(&event.payload))
                found = payload;
        return found;
    }
};
}  // namespace

TEST_CASE("Pvp room does not win on the first Process frame")
{
    PvpFixture f;
    CHECK_FALSE(f.state().m_victory);
    CHECK_FALSE(f.state().m_failure);
    f.ticks(10);
    CHECK_FALSE(f.state().m_victory);
    CHECK_FALSE(f.state().m_failure);
    CHECK(f.state().m_state == mugen::RoomState::Process);
}

TEST_CASE("Pvp KO declares the surviving camp the winner")
{
    PvpFixture f;
    f.m_world.ecs().get<Vitals>(f.m_enemy).hp = {};
    f.ticks(1);
    CHECK(f.state().m_ko);
    CHECK(f.state().m_winnerCamp == 2);
    CHECK_FALSE(f.m_world.ecs().get<Vitals>(f.m_hero).m_beHitEnabled);
    CHECK_FALSE(f.m_world.ecs().get<Vitals>(f.m_enemy).m_beHitEnabled);
    const auto* end = f.lastEvent<PvpRoundEnd>();
    REQUIRE(end != nullptr);
    CHECK(end->m_winnerCamp == 2);
    CHECK(end->m_reason == PvpRoundEndReason::Ko);
    f.ticks(100);
    CHECK(f.state().m_victory);
    CHECK_FALSE(f.state().m_failure);
}

TEST_CASE("Pvp KO against local camp emits Failure")
{
    PvpFixture f;
    f.m_world.ecs().get<Vitals>(f.m_hero).hp = {};
    f.ticks(1);
    CHECK(f.state().m_ko);
    CHECK(f.state().m_winnerCamp == 1);
    const auto* end = f.lastEvent<PvpRoundEnd>();
    REQUIRE(end != nullptr);
    CHECK(end->m_winnerCamp == 1);
    CHECK(end->m_reason == PvpRoundEndReason::Ko);
    f.ticks(100);
    CHECK(f.state().m_failure);
    CHECK_FALSE(f.state().m_victory);
}

TEST_CASE("Pvp timeout picks the higher remaining HP ratio")
{
    SUBCASE("Local camp wins by ratio")
    {
        PvpFixture f;
        f.m_world.ecs().get<Vitals>(f.m_hero).hp  = "90"_fx;
        f.m_world.ecs().get<Vitals>(f.m_enemy).hp = "50"_fx;
        f.state().m_pvpTimeLimit                   = {};
        f.ticks(1);
        CHECK(f.state().m_winnerCamp == 2);
        const auto* end = f.lastEvent<PvpRoundEnd>();
        REQUIRE(end != nullptr);
        CHECK(end->m_reason == PvpRoundEndReason::Timeout);
        f.ticks(5);
        CHECK(f.state().m_victory);
        CHECK_FALSE(f.state().m_failure);
    }
    SUBCASE("Local camp loses by ratio")
    {
        PvpFixture f;
        f.m_world.ecs().get<Vitals>(f.m_hero).hp  = "50"_fx;
        f.m_world.ecs().get<Vitals>(f.m_enemy).hp = "90"_fx;
        f.state().m_pvpTimeLimit                   = {};
        f.ticks(1);
        CHECK(f.state().m_winnerCamp == 1);
        const auto* end = f.lastEvent<PvpRoundEnd>();
        REQUIRE(end != nullptr);
        CHECK(end->m_reason == PvpRoundEndReason::Timeout);
        f.ticks(5);
        CHECK(f.state().m_failure);
        CHECK_FALSE(f.state().m_victory);
    }
    SUBCASE("Equal ratio is a draw treated as failure")
    {
        PvpFixture f;
        f.m_world.ecs().get<Vitals>(f.m_hero).hp  = "50"_fx;
        f.m_world.ecs().get<Vitals>(f.m_enemy).hp = "50"_fx;
        f.state().m_pvpTimeLimit                   = {};
        f.ticks(1);
        CHECK(f.state().m_winnerCamp == 0);
        const auto* end = f.lastEvent<PvpRoundEnd>();
        REQUIRE(end != nullptr);
        CHECK(end->m_reason == PvpRoundEndReason::Timeout);
        f.ticks(5);
        CHECK(f.state().m_failure);
        CHECK_FALSE(f.state().m_victory);
    }
}

TEST_CASE("Pvp room has no revive budget and ignores revive commands")
{
    PvpFixture f;
    CHECK(f.state().m_maxRevives == 0);
    f.state().m_maxRevives = 1;
    f.m_world.ecs().get<RoleStatus>(f.m_hero).eEntityStatus = EntityRoleStatus::Death;
    f.state().m_reviving                                    = true;
    FrameInput input;
    input.frame = f.m_world.ecs().singleton<WorldClock>().frame + 1;
    input.systemCommands.push_back(ReviveCommand{0});
    f.m_world.tick(input);
    CHECK(f.state().m_usedRevives == 0);
    CHECK(f.m_world.ecs().get<RoleStatus>(f.m_hero).eEntityStatus == EntityRoleStatus::Death);
}

TEST_CASE("Pvp bootstrap scales player HP by m_pvpHpScale")
{
    test::ScenarioHarness::loadAssets();
    const int32_t roleId      = 101;
    const auto* role          = conf::Config::instance().find<conf::RoleConfig>(roleId);
    REQUIRE(role != nullptr);
    const Attributes base     = RoleSpawner::loadAttributes(*role, 1);

    World world;
    auto setup               = test::ScenarioHarness::roomSetup(30002);
    PlayerSetup player;
    player.m_class           = CharacterClass::kSwordman;
    player.m_roleId          = roleId;
    player.m_level           = 1;
    player.m_camp            = 2;
    player.m_slot            = 0;
    setup.m_players.push_back(player);
    setup.m_pvp              = true;
    setup.m_pvpHpScale       = "2"_fx;
    const auto heroes        = bootstrapBattle(world, setup);
    REQUIRE(heroes.size() == 1);
    const Entity hero        = heroes.front();
    REQUIRE(hero.value != 0);
    const auto& vitals       = world.ecs().get<Vitals>(hero);
    CHECK(vitals.maxHp == base.maxHp() * "2"_fx);
    CHECK(vitals.hp == vitals.maxHp);
    const auto& attributes   = world.ecs().get<Attributes>(hero);
    CHECK(attributes.basic[4] == base.basic[4] * "2"_fx);
}
