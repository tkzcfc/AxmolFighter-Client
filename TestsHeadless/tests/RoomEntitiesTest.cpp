#include "SkillTestSupport.h"
#include "mugen/sim/room/RoomFlow.h"
#include "mugen/sim/obstacle/Obstacle.h"
#include "mugen/sim/hit/Hit.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "../support/ScenarioHarness.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
using namespace skill_test;
namespace
{
void tick(World& world)
{
    FrameInput input;
    input.frame = world.ecs().singleton<WorldClock>().frame + 1;
    world.tick(input);
}
Entity hero(World& world)
{
    RoleSpawnParams params;
    params.roleId = 0;
    params.camp = 2;
    params.playerSlot = 0;
    return spawnRole(world, params);
}
void box(World& world, Entity entity, Fixed x)
{
    auto& collider = world.ecs().get<ColliderState>(entity);
    collider.radius = "20"_fx;
    collider.registrationFrame = 0;
    collider.damage = {{{x, {}, {}}, {"20"_fx, {}, "20"_fx}}};
    collider.attack = collider.damage;
}
// 场景脚本将攻击输入转换为确定性致死伤害，隔离房间推进而不测试技能平衡。
void scriptedRole(World& world, Entity entity)
{
    auto& ecs = world.ecs();
    if (!ecs.get<RoleIdentity>(entity).playerSlot) return;
    auto& commands = ecs.get<ControlEventQueue>(entity).commands;
    for (const auto& command : commands)
    {
        if (command.controlEvent == ControlEvent::CTR_ATKA)
            ecs.each<RoleIdentity, Vitals>([&](Entity target, const RoleIdentity& role, Vitals& vitals) {
                if (role.roleType >= 2 && role.roleType <= 4)
                {
                    Role(world, target).modifyHp(-vitals.hp, {}, true);
                    world.destroy(target);
                }
            });
        else if (command.controlEvent == ControlEvent::CTR_RUN)
            ecs.each<PortalState, Transform>([&](Entity, const PortalState& state, const Transform& transform) {
                if (state.m_phase == PortalPhase::Active) ecs.get<Transform>(entity).position = transform.position;
            });
    }
    commands.clear();
    auto& collider = ecs.get<ColliderState>(entity);
    const auto p = ecs.get<Transform>(entity).position;
    collider.radius = "20"_fx;
    collider.damage = {{{p.x - "10"_fx, {}, p.z - "10"_fx}, {"20"_fx, {}, "20"_fx}}};
}
}
TEST_CASE("Portal hook-driven room-flow integration copy101 stage40101 visits both rooms")
{
    test::ScenarioHarness::loadAssets();
    WorldHooks hooks;
    hooks.update[0] = scriptedRole;
    World world(hooks);
    world.init({});
    const auto player = hero(world);
    const auto& copy = conf::Config::instance().get<conf::CopyConfig>(101);
    const auto& stage = conf::Config::instance().get<conf::StageConfig>(copy.stageId);
    REQUIRE(stage.id == 40101);
    REQUIRE(stage.roomId == 2010101);
    REQUIRE(room::enter(world, conf::Config::instance().get<conf::RoomConfig>(stage.roomId), stage.id, copy.id));
    for (int i = 0; i < 240; ++i)
    {
        FrameInput input;
        input.frame = world.ecs().singleton<WorldClock>().frame + 1;
        const auto& state = world.ecs().singleton<sim::RoomState>();
        if (state.m_state == mugen::RoomState::ReadySpine) input.systemCommands.push_back(RoomWarningDone{state.roomId});
        PlayerCommand command;
        command.controlEvent = state.m_victory ? ControlEvent::CTR_RUN : ControlEvent::CTR_ATKA;
        input.players.push_back({0, {command}});
        world.tick(input);
        const auto& next = world.ecs().singleton<sim::RoomState>();
        if (next.roomId == 2010102 && next.m_state == mugen::RoomState::End) break;
    }
    const auto& state = world.ecs().singleton<sim::RoomState>();
    CHECK(state.roomId == 2010102);
    CHECK(state.m_victory);
    CHECK(state.m_state == mugen::RoomState::End);
    CHECK(state.m_clearedRooms == std::vector<int32_t>{2010101, 2010102});
    CHECK(world.ecs().singleton<PlayerSlots>().entities.at(0) == player);
}
TEST_CASE("Portal Closed rejects contact then FightEnd opens and transfer preserves player pools")
{
    loadConfig();
    WorldHooks hooks;
    hooks.update[0] = WorldHooks::noEntityUpdate;
    World world(hooks);
    world.init({});
    const auto player = hero(world);
    conf::RoomConfig config;
    config.id = 20001;
    config.portalIds = {101}; config.portalDestType = {2};
    config.portalDestRoomIds = {{20002}}; config.portalDestPosX = {{400}}; config.portalDestPosZ = {{220}};
    REQUIRE(room::enter(world, config, 0, 0));
    Entity door;
    world.ecs().each<PortalState>([&](Entity entity, const PortalState&) { door = entity; });
    REQUIRE(door.value != 0);
    world.ecs().get<EntityGroup>(player).suspended = false;
    room_entities::RoomEntity(world, door).onCollision(player, CollisionEvent::Continue);
    CHECK(world.ecs().get<PortalState>(door).m_phase == PortalPhase::Close);
    CHECK(world.ecs().singleton<sim::RoomState>().m_transferRoom == 0);
    for (int i = 0; i < 12; ++i) tick(world);
    REQUIRE(world.ecs().get<PortalState>(door).m_phase == PortalPhase::Active);
    world.ecs().get<RoleIdentity>(player).roleType = 3;
    auto& vitals = world.ecs().get<Vitals>(player);
    vitals.hp = "123"_fx; vitals.ep = "456"_fx;
    auto& skills = world.ecs().get<SkillPool>(player);
    REQUIRE_FALSE(skills.m_skills.empty());
    skills.m_skills[0].m_cd = "789"_fx;
    world.ecs().get<RoleStatus>(player).entityParams.m_path = {{"100"_fx, {}, {}}};
    world.ecs().get<RoleHitState>(player).forceVelocity.x = "3"_fx;
    world.ecs().get<MotionState>(door).motionFile.clear();
    box(world, door, {}); box(world, player, {});
    world.ecs().get<Transform>(player).position = {};
    tick(world);
    REQUIRE(world.ecs().get<PortalState>(door).m_phase == PortalPhase::Transfer);
    tick(world);
    CHECK(world.ecs().singleton<sim::RoomState>().roomId == 20002);
    CHECK(world.ecs().singleton<PlayerSlots>().entities.at(0) == player);
    CHECK(world.ecs().get<Vitals>(player).hp == "123"_fx);
    CHECK(world.ecs().get<Vitals>(player).ep == "456"_fx);
    CHECK(world.ecs().get<SkillPool>(player).m_skills[0].m_cd == "789"_fx);
    CHECK(world.ecs().get<RoleStatus>(player).entityParams.m_path.empty());
    CHECK(world.ecs().get<RoleHitState>(player).forceVelocity.x == Fixed{});
    CHECK(world.ecs().get<ColliderState>(player).damage.empty());
}
TEST_CASE("Obstacle contact corrects position and friendly effect destroys hit-count obstacle")
{
    loadConfig();
    World world;
    world.init({});
    const auto player = hero(world);
    const auto& rows = conf::Config::instance().table<conf::ObstacleConfig>();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const auto& value) { return value.hitCounts > 0; });
    REQUIRE(row != rows.end());
    const auto wall = room_entities::obstacle(world, row->id, {}, 1);
    box(world, player, "-10"_fx); box(world, wall, {});
    world.ecs().get<Transform>(player).position.x = "-5"_fx;
    obstacle::onCollision(world, player, wall, CollisionEvent::Began, {});
    CHECK(world.ecs().get<Transform>(player).position.x == "-15"_fx);
    world.ecs().get<ObstacleState>(wall).m_hits = 1;
    world.ecs().get<MotionState>(wall).motionFile.clear();
    const auto effect = effect::EffectActor::create(world, player, 920000);
    REQUIRE(effect.value != 0);
    hit::onEffectCollision(world, effect, wall, CollisionEvent::Began, {});
    CHECK(world.ecs().get<ObstacleState>(wall).m_hits == 0);
    for (int i = 0; i < 120 && !world.ecs().get<EntityGroup>(wall).destroyRequested; ++i)
        room_entities::RoomEntity::update(world, wall);
    CHECK(world.ecs().get<EntityGroup>(wall).destroyRequested);
}
TEST_CASE("Goods monster Death drops Spurt item and Idle pickup applies configured buff")
{
    test::ScenarioHarness::loadAssets();
    World world;
    world.init({});
    const auto player = hero(world);
    RoleSpawnParams params;
    params.roleId = 2223;
    const auto enemy = spawnRole(world, params);
    REQUIRE(enemy.value != 0);
    for (const auto& [path, map] : AvatarAssetCache::getInstance()->motionMaps)
        if (const auto* motion = map.findMotion("death"); motion && motion->durationMs() > 0)
        { world.ecs().get<MotionState>(enemy).motionFile = path; break; }
    world.ecs().add(enemy, DropGoods{{6}, {}, {}});
    world.ecs().get<Vitals>(enemy).hp = {};
    Role(world, enemy).changeStatus(EntityRoleStatus::Death, -1);
    Entity item;
    for (int i = 0; i < 180 && item.value == 0; ++i)
    {
        tick(world);
        world.ecs().each<GoodsState>([&](Entity entity, const GoodsState&) { item = entity; });
    }
    REQUIRE(item.value != 0);
    CHECK(world.ecs().get<GoodsState>(item).m_phase == GoodsPhase::Spurt);
    room_entities::RoomEntity(world, item).onCollision(player, CollisionEvent::Began);
    CHECK(world.ecs().get<GoodsState>(item).m_phase == GoodsPhase::Spurt);
    for (int i = 0; i < 120 && world.ecs().get<GoodsState>(item).m_phase == GoodsPhase::Spurt; ++i)
        room_entities::RoomEntity::update(world, item);
    REQUIRE(world.ecs().get<GoodsState>(item).m_phase == GoodsPhase::Idle);
    world.ecs().get<Vitals>(player).hp = "1"_fx;
    room_entities::RoomEntity(world, item).onCollision(player, CollisionEvent::Began);
    CHECK(world.ecs().get<GoodsState>(item).m_phase == GoodsPhase::Collect);
    CHECK(world.ecs().get<GoodsState>(item).m_collector == player);
    CHECK(world.ecs().get<Vitals>(player).hp > "1"_fx);
}
