#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/ai/AiRules.h"
#include "mugen/sim/ai/AiComponents.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/AiDef.h"
#include "mugen/conf/battle/RoleDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include <algorithm>

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

World makeAiWorld()
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 2010101;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(2010101);
    REQUIRE(room != nullptr);
    setup.mapKey = room->mapKey;
    PlayerSetup player;
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.aiEnabled = true;
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

Entity findMonster(World& world)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.kind != SpawnKind::Role || identity.playerSlot.has_value())
            return;
        const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
        if (role && !role->aiId.empty() && role->roleType >= 2)
            found = entity;
    });
    return found;
}

void clearSkillAiLoad(World& world, Entity monster)
{
    auto& book = world.ecs().get<SkillBookComponent>(monster);
    for (auto& skill : book.skills)
    {
        skill.aiLoadCd  = {};
        skill.aiCheckCd = {};
    }
}
}  // namespace

TEST_CASE("ai monster facing stays on the player while chasing and backing up")
{
    World world          = makeAiWorld();
    const Entity hero    = roleByConfig(world, 101);
    const Entity monster = findMonster(world);
    REQUIRE(hero.value != 0);
    REQUIRE(monster.value != 0);
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    REQUIRE(world.ecs().has<AiBrainComponent>(monster));

    auto& brain = world.ecs().get<AiBrainComponent>(monster);
    brain.alerted         = true;
    brain.target          = hero;
    brain.skillIntervalMs = {Fixed::fromInt(60000)};
    clearSkillAiLoad(world, monster);

    const auto* ai = conf::Config::instance().find<conf::battle::AiDef>(brain.aiId);
    REQUIRE(ai != nullptr);
    const int32_t chaseMin = ai->chaseScopeX.size() >= 1 ? ai->chaseScopeX[0] : 80;
    const int32_t chaseMax = ai->chaseScopeX.size() >= 2 ? ai->chaseScopeX[1] : 200;

    auto runFacing = [&](int32_t offsetX) {
        auto& monsterPos = world.ecs().get<TransformComponent>(monster).position;
        world.ecs().get<TransformComponent>(hero).position = monsterPos;
        world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(offsetX);
        int32_t lastToward = 0;
        bool flipped       = false;
        for (int frame = 0; frame < 120; ++frame)
        {
            world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
            const auto& transform = world.ecs().get<TransformComponent>(monster);
            const auto& heroPos   = world.ecs().get<TransformComponent>(hero).position;
            const Fixed dx        = heroPos.x - transform.position.x;
            if (dx == Fixed{})
                continue;
            const int32_t expected = dx > Fixed{} ? 1 : -1;
            CHECK(transform.towardX == expected);
            if (lastToward != 0 && transform.towardX != lastToward)
                flipped = true;
            lastToward = transform.towardX;
        }
        CHECK_FALSE(flipped);
    };

    runFacing(chaseMax + 80);
    runFacing((chaseMin + chaseMax) / 2);
    runFacing(std::max(20, chaseMin / 2));
}

TEST_CASE("ai SkillAi oppDisX blocks out-of-range attacks and allows in-range ones")
{
    World world          = makeAiWorld();
    const Entity hero    = roleByConfig(world, 101);
    const Entity monster = findMonster(world);
    REQUIRE(hero.value != 0);
    REQUIRE(monster.value != 0);
    ai::rules::ensureBrain(world, monster, 1);
    auto& brain = world.ecs().get<AiBrainComponent>(monster);
    brain.alerted = true;
    brain.target  = hero;
    for (auto& interval : brain.skillIntervalMs)
        interval = {};
    if (brain.skillIntervalMs.empty())
        brain.skillIntervalMs.push_back({});

    auto& book = world.ecs().get<SkillBookComponent>(monster);
    REQUIRE_FALSE(book.skills.empty());
    const conf::battle::SkillAiDef* row = nullptr;
    for (const auto& item : conf::Config::instance().table<conf::battle::SkillAiDef>())
    {
        if (item.oppDisX.size() < 2 || item.prob <= 0)
            continue;
        bool hasDisX = false;
        bool extra   = false;
        for (const int32_t cond : item.mustAll)
        {
            hasDisX |= cond == 1;
            extra |= cond != -1 && cond != 1;
        }
        for (const int32_t cond : item.mustAny)
        {
            hasDisX |= cond == 1;
            extra |= cond != -1 && cond != 1;
        }
        if (hasDisX && !extra)
        {
            row = &item;
            break;
        }
        if (hasDisX && row == nullptr)
            row = &item;
    }
    REQUIRE(row != nullptr);
    REQUIRE(row->oppDisX.size() >= 2);
    for (auto& skill : book.skills)
    {
        skill::rules::initSkillAiRuntime(world, monster, skill, row->id, true);
        skill.aiLoadCd  = {};
        skill.aiCheckCd = {};
    }

    DrawPolicy policy;
    world.setDrawPolicy(&policy);
    auto& monsterTransform = world.ecs().get<TransformComponent>(monster);
    auto& heroTransform    = world.ecs().get<TransformComponent>(hero);
    world.ecs().get<IntentComponent>(monster).target = hero;

    const int32_t far = row->oppDisX[1] + 400;
    heroTransform.position = monsterTransform.position;
    heroTransform.position.x += Fixed::fromInt(far);
    CHECK_FALSE(skill::rules::isAllowCast(world, monster, 0, true));
    clearSkillAiLoad(world, monster);
    ai::rules::updateAgent(world, monster);
    CHECK(world.ecs().get<IntentComponent>(monster).presetSkillIndex == kNoSkill);

    const int32_t mid = (row->oppDisX[0] + row->oppDisX[1]) / 2;
    heroTransform.position = monsterTransform.position;
    heroTransform.position.x += Fixed::fromInt(std::max(1, mid));
    for (auto& interval : brain.skillIntervalMs)
        interval = {};
    clearSkillAiLoad(world, monster);
    CHECK(skill::rules::isAllowCast(world, monster, 0, true));
    clearSkillAiLoad(world, monster);
    ai::rules::updateAgent(world, monster);
    CHECK(world.ecs().get<IntentComponent>(monster).presetSkillIndex >= 0);
}
