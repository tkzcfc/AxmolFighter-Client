#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/ClipDef.h"
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
        if (identity.configId == configId && identity.kind == SpawnKind::Role &&
            !world.ecs().has<InactiveComponent>(entity))
            found = entity;
    });
    return found;
}
}  // namespace

TEST_CASE("transform system inactivates old form and links the new role")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    REQUIRE(conf::Config::instance().find<conf::battle::RoleDef>(102) != nullptr);
    const Fixed hp = world.ecs().get<HealthComponent>(hero).hp;
    const auto pos = world.ecs().get<TransformComponent>(hero).position;

    TransformRequest request;
    request.entity       = hero;
    request.targetRoleId = 102;
    request.type         = 0;
    world.ecs().singleton<EventQueue<TransformRequest>>().push(request);
    runTransform(world);

    CHECK(world.ecs().alive(hero));
    CHECK(world.ecs().has<InactiveComponent>(hero));
    CHECK_FALSE(world.ecs().has<PlayerControlComponent>(hero));
    const Entity neu = roleByConfig(world, 102);
    REQUIRE(neu.value != 0);
    CHECK(neu.value != hero.value);
    CHECK(world.ecs().has<TransformLinkComponent>(hero));
    CHECK(world.ecs().has<TransformLinkComponent>(neu));
    const auto& oldLink = world.ecs().get<TransformLinkComponent>(hero);
    const auto& newLink = world.ecs().get<TransformLinkComponent>(neu);
    REQUIRE(oldLink.members.size() == 2);
    CHECK(oldLink.members == newLink.members);
    CHECK(world.ecs().get<HealthComponent>(neu).hp == hp);
    CHECK(world.ecs().get<TransformComponent>(neu).position.x == pos.x);
    CHECK(world.ecs().get<IdentityComponent>(neu).camp == 2);
    bool sawSwap = false;
    for (const auto& cue : world.ecs().singleton<EventQueue<CueEvent>>().items)
        if (cue.kind == CueKind::TransformSwap && cue.entity == neu)
            sawSwap = true;
    CHECK(sawSwap);
}

TEST_CASE("transform cue on role clip emits TransformRequest once")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);

    int32_t clipId      = 0;
    int32_t transformId = 0;
    for (const auto& clip : conf::Config::instance().table<conf::battle::ClipDef>())
    {
        for (const auto& cue : clip.cues)
        {
            if (cue.kind == conf::battle::ClipCueKind::Transform && cue.transformId > 0 &&
                conf::Config::instance().find<conf::battle::RoleDef>(cue.transformId) != nullptr)
            {
                clipId      = clip.id;
                transformId = cue.transformId;
                break;
            }
        }
        if (clipId != 0)
            break;
    }
    REQUIRE(clipId != 0);

    auto& book = world.ecs().get<SkillBookComponent>(hero);
    REQUIRE_FALSE(book.skills.empty());
    world.ecs().get<SkillCastComponent>(hero).skill = 0;
    world.ecs().get<RoleStateComponent>(hero).state = RoleStateId::Attack;
    auto& timeline   = world.ecs().get<ActionTimelineComponent>(hero);
    timeline.clips = {clipId};
    REQUIRE(timeline::rules::enterClip(world, hero, clipId, false, false));

    int32_t count = 0;
    for (int i = 0; i < 180; ++i)
    {
        timeline::rules::advanceClip(world, hero);
        for (const auto& request : world.ecs().singleton<EventQueue<TransformRequest>>().items)
            if (request.entity == hero && request.targetRoleId == transformId)
                ++count;
        if (count > 0)
            break;
    }
    CHECK(count == 1);
    timeline::rules::advanceClip(world, hero);
    int32_t again = 0;
    for (const auto& request : world.ecs().singleton<EventQueue<TransformRequest>>().items)
        if (request.entity == hero && request.targetRoleId == transformId)
            ++again;
    CHECK(again == 1);
}
