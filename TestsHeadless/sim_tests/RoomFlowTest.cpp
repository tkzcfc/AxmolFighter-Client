#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/view/HudView.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;
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

World makeWorld(int32_t roomId)
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = roomId;
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

Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == configId && found.value == 0)
            found = entity;
    });
    return found;
}

void tickN(World& world, uint64_t frames)
{
    for (uint64_t i = 0; i < frames; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
}
}  // namespace

TEST_CASE("room: ReturnToTownCommand is honored after dungeon victory")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    tickN(world, 12);
    CHECK(room.state == mugen::RoomState::Process);

    world.ecs().each<IdentityComponent, HealthComponent>(
        [&](Entity entity, const IdentityComponent& identity, HealthComponent& health) {
            if (!identity.playerSlot && identity.kind == SpawnKind::Role && health.hp > Fixed{})
                health::rules::applyDelta(world, entity, -health.hp, HpCause::Hit, entity, {});
        });
    tickN(world, 40);
    REQUIRE(room.victory);
    REQUIRE(room.state != mugen::RoomState::Process);

    FrameInput input;
    input.frame = world.frame() + 1;
    input.systemCommands.push_back(ReturnToTownCommand{0});
    world.tick(input);
    CHECK(room.returnToTown);
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    CHECK(view::readBattleRoom(world, hero).returnToTown);
}
