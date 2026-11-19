#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

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

// 把英雄移到指定坐标并重新采样受击盒。
void placeHero(World& world, Entity hero, const FixedVec3& position)
{
    world.ecs().get<TransformComponent>(hero).position = position;
    runHitbox(world);
}
}  // namespace

TEST_CASE("Room 20002 portals keep an empty motion file and a radius damage box")
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 20002;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(setup.roomId);
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

    for (int i = 0; i < 30; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});

    int32_t portals = 0;
    world.ecs().each<PortalStateComponent>([&](Entity entity, const PortalStateComponent& state) {
        ++portals;
        const auto& motion = world.ecs().get<MotionStateComponent>(entity);
        CHECK(motion.motionFile.empty());
        CHECK_FALSE(motion.motionName.empty());
        const auto* row = conf::Config::instance().find<conf::PortalConfig>(state.configId);
        REQUIRE(row != nullptr);
        CHECK(motion.motionName == room::rules::portalMotion(*row, PortalPhase::Close));
        CHECK_FALSE(world.ecs().get<HitboxesComponent>(entity).damage.empty());
    });
    REQUIRE(portals > 0);
}

TEST_CASE("dungeon portal motion follows close, active, then transfer")
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 2010101;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(setup.roomId);
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

    Entity hero;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == 101)
            hero = entity;
    });
    REQUIRE(hero.value != 0);
    Entity portal;
    world.ecs().each<PortalStateComponent>([&](Entity entity, const PortalStateComponent& state) {
        if (portal.value == 0 && state.roomId != 0)
            portal = entity;
    });
    REQUIRE(portal.value != 0);

    const auto& state = world.ecs().get<PortalStateComponent>(portal);
    const auto* row   = conf::Config::instance().find<conf::PortalConfig>(state.configId);
    REQUIRE(row != nullptr);
    const auto& closeMotion = world.ecs().get<MotionStateComponent>(portal);
    CHECK(closeMotion.motionFile.empty());
    CHECK(closeMotion.motionName == room::rules::portalMotion(*row, PortalPhase::Close));
    CHECK(closeMotion.loop);
    CHECK(closeMotion.playing);

    for (int i = 0; i < 8; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    world.ecs().singleton<mugen::sim::RoomState>().victory = true;
    room::rules::openPortals(world);
    const auto& activeMotion = world.ecs().get<MotionStateComponent>(portal);
    CHECK(world.ecs().get<PortalStateComponent>(portal).phase == PortalPhase::Active);
    CHECK(activeMotion.motionName == room::rules::portalMotion(*row, PortalPhase::Active));
    CHECK(activeMotion.loop);
    CHECK(activeMotion.playing);

    placeHero(world, hero, {Fixed::fromInt(1600), {}, Fixed::fromInt(130)});
    runInteract(world);
    const auto& transferMotion = world.ecs().get<MotionStateComponent>(portal);
    CHECK(world.ecs().get<PortalStateComponent>(portal).phase == PortalPhase::Transfer);
    CHECK(transferMotion.motionName == room::rules::portalMotion(*row, PortalPhase::Transfer));
    CHECK_FALSE(transferMotion.loop);
    CHECK(transferMotion.playing);
}
