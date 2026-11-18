#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/battle/ClipDef.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/conf/battle/RoleDef.h"
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

int32_t pickSummonRoleId()
{
    const auto& rows = conf::Config::instance().table<conf::battle::RoleDef>();
    for (const auto& row : rows)
        if (row.roleType == static_cast<int32_t>(EntityRoleType::kSummon))
            return row.id;
    for (const auto& row : rows)
        if (row.roleType >= 2 && row.roleType <= 4 && row.id != 61300 && row.id != 61320)
            return row.id;
    return 0;
}
}  // namespace

TEST_CASE("summon spawn inherits owner camp and expires by lifetime")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const int32_t roleId = pickSummonRoleId();
    REQUIRE(roleId != 0);
    const auto& heroId = world.ecs().get<IdentityComponent>(hero);
    const auto& heroTf = world.ecs().get<TransformComponent>(hero);

    SpawnRequest request;
    request.kind       = SpawnKind::Role;
    request.configId   = roleId;
    request.owner      = hero;
    request.position   = heroTf.position;
    request.towardX    = heroTf.towardX;
    request.lifetimeMs = 100;
    request.level      = heroId.level;
    request.camp       = heroId.camp;
    request.isAi       = true;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
    life::rules::spawnFlush(world, SpawnStage::Lifecycle);

    Entity summoned;
    world.ecs().each<SummonComponent, IdentityComponent>([&](Entity entity, const SummonComponent& summon,
                                                             const IdentityComponent& identity) {
        if (summon.owner == hero && identity.configId == roleId)
            summoned = entity;
    });
    REQUIRE(summoned.value != 0);
    CHECK(world.ecs().get<IdentityComponent>(summoned).camp == heroId.camp);
    CHECK(world.ecs().get<SummonComponent>(summoned).root == hero);
    CHECK(world.ecs().get<SummonComponent>(summoned).remainingMs == Fixed::fromInt(100));

    bool expired = false;
    const uint64_t start = world.frame();
    for (uint64_t frame = start + 1; frame <= start + 20; ++frame)
    {
        world.tick(FrameInput{frame, {}, {}, {}});
        if (!world.ecs().alive(summoned) || world.ecs().get<HealthComponent>(summoned).dead ||
            world.ecs().get<RoleStateComponent>(summoned).deathRequested)
        {
            expired = true;
            break;
        }
    }
    CHECK(expired);
}

TEST_CASE("summon cue on effect clip queues a role SpawnRequest")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);

    int32_t clipId   = 0;
    int32_t summonId = 0;
    for (const auto& clip : conf::Config::instance().table<conf::battle::EffectClipDef>())
    {
        for (const auto& cue : clip.cues)
        {
            if (cue.kind == conf::battle::ClipCueKind::Summon && cue.summonId > 0)
            {
                clipId   = clip.id;
                summonId = cue.summonId;
                break;
            }
        }
        if (clipId != 0)
            break;
    }
    REQUIRE(clipId != 0);
    REQUIRE(conf::Config::instance().find<conf::battle::RoleDef>(summonId) != nullptr);

    int32_t effectId = 0;
    for (const auto& row : conf::Config::instance().table<conf::battle::EffectDef>())
    {
        if (row.id > 0)
        {
            effectId = row.id;
            break;
        }
    }
    REQUIRE(effectId != 0);
    effect::rules::spawnFromRole(world, hero, effectId, {});
    life::rules::spawnFlush(world, SpawnStage::RoleTimeline);

    Entity effect;
    world.ecs().each<EffectInfoComponent>([&](Entity entity, const EffectInfoComponent& info) {
        if (info.configId == effectId && info.owner == hero)
            effect = entity;
    });
    REQUIRE(effect.value != 0);
    auto& timeline    = world.ecs().get<ActionTimelineComponent>(effect);
    timeline.clips  = {clipId};
    timeline.index  = 0;
    REQUIRE(timeline::rules::enterClip(world, effect, clipId, true, false));

    bool spawned = false;
    for (int i = 0; i < 120 && !spawned; ++i)
    {
        const bool done = timeline::rules::advanceClip(world, effect);
        for (const auto& request : world.ecs().singleton<EventQueue<SpawnRequest>>().items)
            if (request.kind == SpawnKind::Role && request.configId == summonId && request.owner == hero)
                spawned = true;
        if (done)
            break;
    }
    CHECK(spawned);
}
