#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/ClipDef.h"
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
    player.m_roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.m_level  = 1000;
    player.m_camp   = 2;
    player.m_slot   = 0;
    player.m_isAi   = false;
    setup.m_players.push_back(player);
    setup.m_stageId   = 0;
    setup.m_copyId    = 0;
    setup.m_aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.m_configId == configId)
            found = entity;
    });
    return found;
}
}  // namespace

TEST_CASE("clip floor=0 EndClip finishes 920800 on landing before tail delay")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    int32_t index     = kNoSkill;
    for (int32_t i = 0; i < static_cast<int32_t>(book.m_skills.size()); ++i)
        if (book.m_skills[static_cast<size_t>(i)].m_configId == 920150)
            index = i;
    REQUIRE(index != kNoSkill);
    skill::rules::beginCast(world, hero, index);

    int32_t entered = 0;
    int32_t left    = 0;
    bool sawAir     = false;
    bool sawPolicy  = false;
    for (uint64_t frame = 1; frame <= 200; ++frame)
    {
        world.tick(FrameInput{frame, {}, {}, {}});
        const auto& timeline = world.ecs().get<ActionTimelineComponent>(hero);
        const auto& disp     = world.ecs().get<DisplacementComponent>(hero);
        const auto y         = world.ecs().get<TransformComponent>(hero).m_position.y;
        if (timeline.m_clipId == 920800)
        {
            if (entered == 0)
                entered = static_cast<int32_t>(world.frame());
            if (disp.m_floorPolicy == static_cast<int32_t>(conf::battle::ClipFloorPolicy::EndClip))
                sawPolicy = true;
            if (y > Fixed{})
                sawAir = true;
        }
        else if (entered != 0 && left == 0)
        {
            left = static_cast<int32_t>(world.frame());
            break;
        }
    }
    CHECK(entered > 0);
    CHECK(sawPolicy);
    CHECK(sawAir);
    CHECK(left > entered);
    // 尾延时 5000ms 约 300 帧；落地 EndClip 应在跳跃曲线内结束。
    CHECK(left - entered < 120);
}
