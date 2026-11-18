#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

namespace
{
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

World makeWorld()
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 2071501;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(2071501);
    REQUIRE(room != nullptr);
    setup.mapKey = room->mapKey;
    PlayerSetup player;
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.stageId   = 0;
    setup.copyId    = 0;
    setup.aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == configId && identity.kind == SpawnKind::Role)
            found = entity;
    });
    return found;
}

Entity spawnFollowEffect(World& world, Entity owner, int32_t effectId)
{
    SourceContext source;
    source.skillId     = 920090;
    source.baseSkillId = 920090;
    effect::rules::spawnFromRole(world, owner, effectId, source);
    life::rules::spawnFlush(world, SpawnStage::RoleTimeline);
    Entity found;
    world.ecs().each<EffectInfoComponent>([&](Entity entity, const EffectInfoComponent& info) {
        if (info.configId == effectId && info.owner == owner)
            found = entity;
    });
    return found;
}
}  // namespace

TEST_CASE("follow system snaps effect to owner after the owner moves")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(920210);
    REQUIRE(row != nullptr);
    REQUIRE(row->follow == 1);

    const Entity effect = spawnFollowEffect(world, hero, 920210);
    REQUIRE(effect.value != 0);
    REQUIRE(world.ecs().has<FollowComponent>(effect));
    CHECK(world.ecs().get<FollowComponent>(effect).mode == 1);

    auto& owner = world.ecs().get<TransformComponent>(hero);
    owner.position.x += Fixed::fromInt(80);
    owner.position.y += Fixed::fromInt(12);
    const auto expected = owner;
    runFollow(world);
    TransformComponent placed = world.ecs().get<TransformComponent>(effect);
    effect::rules::placeFrom(world, effect, hero, false);
    const auto& after = world.ecs().get<TransformComponent>(effect);
    CHECK(placed.position.x == after.position.x);
    CHECK(placed.position.y == after.position.y);
    CHECK(placed.position.z == after.position.z);
    CHECK(placed.towardX == expected.towardX);
}

TEST_CASE("follow mode 2 grounds y and pause skips follow")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const Entity effect = spawnFollowEffect(world, hero, 920210);
    REQUIRE(effect.value != 0);

    world.ecs().get<FollowComponent>(effect).mode = 2;
    world.ecs().get<TransformComponent>(hero).position.y = Fixed::fromInt(40);
    runFollow(world);
    CHECK(world.ecs().get<TransformComponent>(effect).position.y == Fixed{});

    const Fixed stuckX = world.ecs().get<TransformComponent>(effect).position.x;
    world.ecs().get<PauseComponent>(effect).paused = true;
    world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(50);
    runFollow(world);
    CHECK(world.ecs().get<TransformComponent>(effect).position.x == stuckX);
}

TEST_CASE("follow skips effect-timeline born entities on the spawn frame")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    SourceContext source;
    source.skillId     = 920090;
    source.baseSkillId = 920090;
    effect::rules::spawnFromRole(world, hero, 920210, source);
    life::rules::spawnFlush(world, SpawnStage::EffectTimeline);
    Entity effect;
    world.ecs().each<EffectInfoComponent>([&](Entity entity, const EffectInfoComponent& info) {
        if (info.configId == 920210 && info.owner == hero)
            effect = entity;
    });
    REQUIRE(effect.value != 0);
    const Fixed stuckX = world.ecs().get<TransformComponent>(effect).position.x;
    world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(80);
    runFollow(world);
    CHECK(world.ecs().get<TransformComponent>(effect).position.x == stuckX);
    CHECK(world.ecs().get<IdentityComponent>(effect).born.stage == SpawnStage::EffectTimeline);
}
