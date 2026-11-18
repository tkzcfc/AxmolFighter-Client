#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

// 结构体 RoomState 与枚举 mugen::RoomState 同名，用别名区分。
using Room = mugen::sim::RoomState;

namespace
{
// 挂载 VFS 并加载配置（幂等）。
void loadAssets()
{
    REQUIRE(test::ensureContentVfs());
    auto& config = conf::Config::instance();
    if (!config.loaded())
        REQUIRE(config.load("/mugen/config/config.bin"));
    auto& avatar = *AvatarAssetCache::getInstance();
    if (!avatar.isLoaded())
        REQUIRE(avatar.load("/mugen/config/avatar.bin"));
}

// 构造指定房间的战斗世界（职业 1 / 角色 101，等级 1000，关 AI）。
World makeWorld(int32_t roomId)
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed    = 31;
    setup.mode    = WorldMode::Battle;
    setup.roomId  = roomId;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(roomId);
    REQUIRE(room != nullptr);
    setup.mapKey = room->mapKey;
    PlayerSetup player;
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 按配置 id 查找角色实体。
Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == configId && found.value == 0)
            found = entity;
    });
    return found;
}

// 推进 frames 帧空输入。
void tickN(World& world, uint64_t frames)
{
    for (uint64_t i = 0; i < frames; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
}
}  // namespace

TEST_CASE("room: state machine reaches Process and emits transitions")
{
    World world  = makeWorld(2010101);
    auto& room   = world.ecs().singleton<Room>();
    CHECK(room.enabled);
    tickN(world, 12);
    CHECK(room.state == mugen::RoomState::Process);
    CHECK(room.recorded.size() >= 5);
    CHECK(std::find(room.recorded.begin(), room.recorded.end(), mugen::RoomState::Process) != room.recorded.end());
}

TEST_CASE("room: hero death with no revives triggers failure")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const Fixed hp = world.ecs().get<HealthComponent>(hero).hp;
    health::rules::applyDelta(world, hero, -hp, HpCause::Hit, hero, {});
    tickN(world, 12);
    CHECK(room.failure);
    CHECK_FALSE(room.victory);
}

TEST_CASE("room: hero death with revive budget waits then revives")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    room.maxRevives = 1;
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const Fixed hp = world.ecs().get<HealthComponent>(hero).hp;
    health::rules::applyDelta(world, hero, -hp, HpCause::Hit, hero, {});
    tickN(world, 10);
    CHECK(room.reviving);
    CHECK(world.ecs().get<RoleStateComponent>(hero).state == RoleStateId::Death);
    // 复活命令经帧输入进入：InputSystem 转成 RoomRequest，导演置复活位，下一帧 FSM 执行转移。
    FrameInput input;
    input.frame = world.frame() + 1;
    input.systemCommands.push_back(ReviveCommand{0});
    world.tick(input);
    CHECK_FALSE(room.reviving);
    CHECK(room.usedRevives == 1);
    CHECK(world.ecs().get<RoleStateComponent>(hero).reviveRequested);
    tickN(world, 1);
    const auto& health = world.ecs().get<HealthComponent>(hero);
    CHECK(health.hp == health.maxHp);
    CHECK_FALSE(health.dead);
    CHECK(world.ecs().get<RoleStateComponent>(hero).state == RoleStateId::Idle);
}

TEST_CASE("room: revive goes through FSM and emits Death->Revive->Idle")
{
    World world = makeWorld(2010101);
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    auto& state = world.ecs().get<RoleStateComponent>(hero);
    state.state           = RoleStateId::Death;
    state.reviveRequested = true;
    runRoleFsm(world);
    CHECK(state.state == RoleStateId::Idle);
    CHECK_FALSE(state.reviveRequested);
    const auto& health = world.ecs().get<HealthComponent>(hero);
    CHECK(health.hp == health.maxHp);
    CHECK_FALSE(health.dead);
    // 复活转移发出两条 StateEvent：Death -> Revive -> Idle。
    bool sawDeathToRevive = false;
    bool sawReviveToIdle  = false;
    for (const auto& event : world.ecs().singleton<EventQueue<StateEvent>>().items)
    {
        if (event.entity != hero)
            continue;
        if (event.from == static_cast<int32_t>(RoleStateId::Death) &&
            event.to == static_cast<int32_t>(RoleStateId::Revive))
            sawDeathToRevive = true;
        if (event.from == static_cast<int32_t>(RoleStateId::Revive) &&
            event.to == static_cast<int32_t>(RoleStateId::Idle))
            sawReviveToIdle = true;
    }
    CHECK(sawDeathToRevive);
    CHECK(sawReviveToIdle);
}

TEST_CASE("input: ReturnToTownCommand reaches the room director")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    tickN(world, 12);
    CHECK(room.state == mugen::RoomState::Process);
    FrameInput input;
    input.frame = world.frame() + 1;
    input.systemCommands.push_back(ReturnToTownCommand{0});
    world.tick(input);
    CHECK(room.returnToTown);
}

TEST_CASE("room: boss KO starts slow motion when bosses are cleared")
{
    World world = makeWorld(2071501);
    auto& room  = world.ecs().singleton<Room>();
    const Entity b0 = roleByConfig(world, 61300);
    const Entity b1 = roleByConfig(world, 61320);
    REQUIRE(b0.value != 0);
    REQUIRE(b1.value != 0);
    CHECK(room.haveBoss);
    health::rules::applyDelta(world, b0, -world.ecs().get<HealthComponent>(b0).hp, HpCause::Hit, b0, {});
    health::rules::applyDelta(world, b1, -world.ecs().get<HealthComponent>(b1).hp, HpCause::Hit, b1, {});
    tickN(world, 10);
    CHECK(room.ko);
    CHECK(world.ecs().singleton<SlowMotion>().ko.active);
}

TEST_CASE("room: obstacle breaks after its hit count is exhausted")
{
    World world = makeWorld(2010101);
    TransformComponent transform;
    transform.position = {Fixed::fromInt(400), {}, Fixed::fromInt(130)};
    transform.towardX  = 1;
    const Entity obstacle = room::rules::spawnObstacle(world, 16, transform, 1);
    REQUIRE(obstacle.value != 0);
    CHECK(world.ecs().get<ObstacleStateComponent>(obstacle).hits == 2);
    // 构造一个特效实体，逐次命中障碍。
    const Entity effect = world.ecs().create();
    IdentityComponent id;
    id.kind       = SpawnKind::Effect;
    id.camp       = 1;
    id.hostileMask = 30;
    world.ecs().add(effect, id);
    world.ecs().add(effect, EffectInfoComponent{});
    world.ecs().add(effect, HitboxesComponent{});
    world.ecs().add(effect, TransformComponent{});

    world.ecs().singleton<EventQueue<ContactEvent>>().push({effect, obstacle, ContactPhase::Began, {}});
    tickN(world, 1);
    CHECK(world.ecs().get<ObstacleStateComponent>(obstacle).hits == 1);
    CHECK_FALSE(world.ecs().has<DespawningComponent>(obstacle));
    world.ecs().singleton<EventQueue<ContactEvent>>().push({effect, obstacle, ContactPhase::Began, {}});
    tickN(world, 1);
    CHECK_FALSE(world.ecs().alive(obstacle));
}

TEST_CASE("room: goods pickup collects and despawns the goods")
{
    World world = makeWorld(2010101);
    const Entity hero = roleByConfig(world, 101);
    const auto& heroPos = world.ecs().get<TransformComponent>(hero).position;
    TransformComponent transform;
    transform.position = heroPos;
    transform.towardX  = 1;
    const Entity goods = room::rules::spawnGoods(world, 6, transform, false);
    REQUIRE(goods.value != 0);
    CHECK(world.ecs().get<GoodsStateComponent>(goods).phase == GoodsPhase::Idle);
    world.ecs().singleton<EventQueue<ContactEvent>>().push({hero, goods, ContactPhase::Began, {}});
    tickN(world, 1);
    CHECK_FALSE(world.ecs().alive(goods));
}

// 把英雄移到指定坐标并重新采样受击盒（传送门触发按受击盒与门半径盒求交）。
void placeHero(World& world, Entity hero, const FixedVec3& position)
{
    world.ecs().get<TransformComponent>(hero).position = position;
    runHitbox(world);
}

// 找到首个带目标房间的传送门。
Entity firstPortal(World& world)
{
    Entity portal;
    world.ecs().each<PortalStateComponent>([&](Entity entity, const PortalStateComponent& state) {
        if (portal.value == 0 && state.roomId != 0)
            portal = entity;
    });
    return portal;
}

TEST_CASE("room: portal triggers within radius after victory, not beyond")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const Entity portal = firstPortal(world);
    REQUIRE(portal.value != 0);
    // 推进几帧让英雄进入待机并采样受击盒。
    tickN(world, 8);
    room.victory = true;
    room::rules::openPortals(world);
    CHECK(world.ecs().get<PortalStateComponent>(portal).phase == PortalPhase::Active);

    // 传送门在 x=1725、半径 200，受击盒左缘 1525；x=500 远离，不触发。
    placeHero(world, hero, {Fixed::fromInt(500), {}, Fixed::fromInt(130)});
    runInteract(world);
    CHECK(world.ecs().get<PortalStateComponent>(portal).phase == PortalPhase::Active);

    // x=1600 进入半径盒内，触发传送。
    placeHero(world, hero, {Fixed::fromInt(1600), {}, Fixed::fromInt(130)});
    runInteract(world);
    CHECK(world.ecs().get<PortalStateComponent>(portal).phase == PortalPhase::Transfer);
}

TEST_CASE("room: portal transfer requests a room change after victory")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    const Entity hero = roleByConfig(world, 101);
    const Entity portal = firstPortal(world);
    REQUIRE(portal.value != 0);
    const int32_t dest = world.ecs().get<PortalStateComponent>(portal).roomId;
    tickN(world, 8);
    room.victory = true;
    room::rules::openPortals(world);
    CHECK(world.ecs().get<PortalStateComponent>(portal).phase == PortalPhase::Active);
    placeHero(world, hero, {Fixed::fromInt(1600), {}, Fixed::fromInt(130)});
    // 第一帧 InteractSystem 切 Transfer 并写换房请求，第二帧 RoomTransferSystem 执行换房。
    tickN(world, 2);
    CHECK(room.roomId == dest);
    CHECK(room.enabled);
}

TEST_CASE("room: defense waves advance after clearing")
{
    World world = makeWorld(2071601);
    auto& room  = world.ecs().singleton<Room>();
    REQUIRE(room.battleRuleType == 2);
    REQUIRE(room.config.battleRuleParam.size() == 3);
    // 击杀首波全部怪物。
    std::vector<Entity> monsters;
    world.ecs().each<IdentityComponent, HealthComponent>(
        [&](Entity entity, const IdentityComponent& identity, HealthComponent& health) {
            if (!identity.playerSlot && identity.kind == SpawnKind::Role && health.hp > Fixed{})
            {
                monsters.push_back(entity);
                health::rules::applyDelta(world, entity, -health.hp, HpCause::Hit, entity, {});
            }
        });
    REQUIRE_FALSE(monsters.empty());
    tickN(world, 80);
    CHECK(room.wave >= 1);
}
