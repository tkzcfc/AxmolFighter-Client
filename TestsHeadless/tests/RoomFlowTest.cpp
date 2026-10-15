#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/sim/room/RoomDirector.h"
#include "mugen/sim/spawn/RoleSpawn.h"

using namespace mugen;
using namespace mugen::sim;
namespace
{
struct RoomFixture
{
    World m_world;
    Entity m_hero, m_enemy;
    RoomFixture(int32_t rule = 1)
    {
        test::ScenarioHarness::loadAssets();
        WorldHooks hooks;
        hooks.update.fill(WorldHooks::noEntityUpdate);
        hooks.collision = WorldHooks::noWorldUpdate;
        m_world = World(hooks);
        m_world.init({});
        m_hero = m_world.create(GroupKind::Role);
        RoleIdentity own;
        own.roleType = 1; own.camp = 2; own.hostileCamp = 1;
        m_world.ecs().add(m_hero, own);
        m_world.ecs().add(m_hero, Transform{});
        m_world.ecs().add(m_hero, Vitals{});
        m_world.ecs().add(m_hero, RoleStatus{});
        m_world.ecs().singleton<PlayerSlots>().entities[0] = m_hero;
        m_enemy = m_world.create(GroupKind::Role);
        RoleIdentity hostile;
        hostile.roleType = 2; hostile.camp = 1;
        m_world.ecs().add(m_enemy, hostile);
        Vitals vitals;
        vitals.hp = "100"_fx;
        m_world.ecs().add(m_enemy, vitals);
        conf::RoomConfig config;
        config.id = 30002; config.battleRuleType = rule;
        config.battleRuleParam = rule == 3 ? std::vector<int32_t>{1} : std::vector<int32_t>{};
        REQUIRE(room::RoomDirector(m_world).enter(config, 0, 0));
        ticks(6);
        REQUIRE(state().m_state == mugen::RoomState::Process);
    }
    sim::RoomState& state() { return m_world.ecs().singleton<sim::RoomState>(); }
    void ticks(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            FrameInput input;
            input.frame = m_world.ecs().singleton<WorldClock>().frame + 1;
            m_world.tick(input);
        }
    }
    bool failureEvent()
    {
        for (const auto& event : m_world.ecs().singleton<PresentationEvents>().events)
            if (std::holds_alternative<Failure>(event.payload)) return true;
        return false;
    }
};
}
TEST_CASE("Room Clear waits for enemy removal then emits Victory")
{
    RoomFixture f;
    f.m_world.ecs().get<Vitals>(f.m_enemy).hp = {};
    f.ticks(1);
    CHECK_FALSE(f.state().m_victory);
    f.m_world.destroy(f.m_enemy);
    f.ticks(2);
    CHECK(f.state().m_victory);
    REQUIRE(f.m_world.ecs().singleton<PresentationEvents>().events.size() >= 2);
    CHECK(std::holds_alternative<Victory>(f.m_world.ecs().singleton<PresentationEvents>().events.back().payload));
}
TEST_CASE("Room Defense countdown and wave count run in simulation")
{
    RoomFixture f(2);
    f.state().m_config.battleRuleParam = {30002};
    f.m_world.destroy(f.m_enemy);
    f.ticks(1);
    CHECK(f.state().m_waveTimer != 0);
    f.ticks(60);
    CHECK(f.state().m_wave == 0);
    f.ticks(1);
    CHECK(f.state().m_wave == 1);
    CHECK_FALSE(f.state().m_victory);
    f.m_world.ecs().each<RoleIdentity>([&](Entity entity, const RoleIdentity& role) {
        if (role.roleType != 1) f.m_world.destroy(entity);
    });
    f.ticks(2);
    CHECK(f.state().m_victory);
}
TEST_CASE("Room Survive wins on time with enemy still present")
{
    RoomFixture f(3);
    f.ticks(32);
    CHECK(f.state().m_victory);
    CHECK(f.m_world.ecs().alive(f.m_enemy));
}
TEST_CASE("Room Clear timeout emits Failure")
{
    RoomFixture f;
    f.state().m_limitSeconds = "0.05"_fx;
    f.ticks(3);
    CHECK(f.state().m_failure);
    CHECK(f.failureEvent());
}
TEST_CASE("Room all bosses dead automatically start KO curve")
{
    RoomFixture f;
    f.state().m_config.roomType = 1;
    f.state().m_haveBoss = true;
    f.m_world.ecs().get<RoleIdentity>(f.m_enemy).roleType = 4;
    f.ticks(1);
    CHECK_FALSE(f.state().m_ko);
    f.m_world.ecs().get<Vitals>(f.m_enemy).hp = {};
    f.ticks(1);
    CHECK(f.state().m_ko);
    CHECK_FALSE(f.m_world.ecs().get<Vitals>(f.m_hero).m_beHitEnabled);
    f.ticks(1);
    CHECK(f.m_world.ecs().singleton<SlowMotion>().scale == "0.1"_fx + ("1"_fx - kLogicDtSec / "0.5"_fx) * "0.9"_fx);
    f.ticks(15);
    CHECK(f.m_world.ecs().singleton<SlowMotion>().scale == "0.1"_fx);
}
TEST_CASE("Room exhausted revive budget and zero budget emit Failure")
{
    RoomFixture f;
    SUBCASE("No revives configured") { f.state().m_maxRevives = 0; }
    SUBCASE("All configured revives consumed") { f.state().m_maxRevives = 3; f.state().m_usedRevives = 3; }
    f.m_world.ecs().get<RoleStatus>(f.m_hero).eEntityStatus = EntityRoleStatus::Death;
    f.state().m_roleDeaths.push_back(f.m_hero);
    f.ticks(2);
    CHECK(f.state().m_failure);
    CHECK(f.failureEvent());
}
TEST_CASE("Room FrameInput revive consumes budget once and rejects further death")
{
    RoomFixture f;
    f.state().m_maxRevives = 1;
    f.m_world.ecs().get<RoleStatus>(f.m_hero).eEntityStatus = EntityRoleStatus::Death;
    f.state().m_roleDeaths.push_back(f.m_hero);
    f.ticks(1);
    REQUIRE(f.state().m_reviving);
    FrameInput input;
    input.frame = f.m_world.ecs().singleton<WorldClock>().frame + 1;
    input.systemCommands = {ReviveCommand{}, ReviveCommand{}};
    f.m_world.tick(input);
    CHECK(f.state().m_usedRevives == 1);
    CHECK_FALSE(f.state().m_reviving);
    CHECK(f.m_world.ecs().get<RoleStatus>(f.m_hero).eEntityStatus == EntityRoleStatus::Revive);
    f.m_world.ecs().get<RoleStatus>(f.m_hero).eEntityStatus = EntityRoleStatus::Death;
    f.state().m_roleDeaths.push_back(f.m_hero);
    f.ticks(2);
    CHECK(f.failureEvent());
}
TEST_CASE("Room FrameInput revive reaches ActionRevive in real role pipeline")
{
    test::ScenarioHarness::loadAssets();
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId = 261;
    params.playerSlot = 0;
    const auto entity = RoleSpawner(world).spawnRole(params);
    world.ecs().get<RoleIdentity>(entity).roleType = 1;
    conf::RoomConfig config;
    config.id = 30002;
    config.battleRuleType = 3;
    config.battleRuleParam = {100};
    REQUIRE(room::RoomDirector(world).enter(config, 0, 0));
    auto& state = world.ecs().singleton<sim::RoomState>();
    state.m_maxRevives = 1;
    state.m_reviving = true;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Death;
    world.ecs().get<Vitals>(entity).hp = {};
    FrameInput input;
    input.frame = 1;
    input.systemCommands.push_back(ReviveCommand{});
    world.tick(input);
    CHECK(state.m_usedRevives == 1);
    CHECK(world.ecs().get<Vitals>(entity).hp == world.ecs().get<Vitals>(entity).maxHp);
    CHECK(world.ecs().get<RoleStatus>(entity).eEntityStatus == EntityRoleStatus::Idle);
}
TEST_CASE("Room transition waits once and return command is slot validated")
{
    RoomFixture f;
    room::RoomDirector(f.m_world).setExpectRoomState(mugen::RoomState::FightVictory);
    room::RoomDirector(f.m_world).setExpectRoomState(mugen::RoomState::FightVictory);
    CHECK(f.state().m_pending.size() == 1);
    CHECK(f.state().m_state == mugen::RoomState::Check);
    CHECK(f.state().m_waitMs == "30"_fx);
    FrameInput input;
    input.frame = f.m_world.ecs().singleton<WorldClock>().frame + 1;
    input.systemCommands.push_back(ReturnToTownCommand{99});
    f.m_world.tick(input);
    CHECK_FALSE(f.state().m_returnToTown);
    input.frame++;
    input.systemCommands = {ReturnToTownCommand{0}};
    f.m_world.tick(input);
    CHECK(f.state().m_returnToTown);
}
