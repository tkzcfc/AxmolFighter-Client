#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/ClipDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include <string>

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
}  // namespace

TEST_CASE("attack intent keeps stick quadrant after clearing run mode")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    auto& intent = world.ecs().get<IntentComponent>(hero);
    intent.move.mode     = MoveMode::Run;
    intent.move.quadrant = ControlQuadrant::DownLeft;
    intent.move.angle    = 0;
    world.ecs().get<RoleStateComponent>(hero).state = RoleStateId::Attack;
    world.ecs().get<PlayerControlComponent>(hero).commands.clear();
    runPlayerIntent(world);
    CHECK(intent.move.mode == MoveMode::None);
    CHECK(intent.move.quadrant == ControlQuadrant::DownLeft);
    CHECK(intent.move.angle == 0);
}

TEST_CASE("free-move clip control faces the stick and writes control velocity")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    const auto* clip = conf::Config::instance().find<conf::battle::ClipDef>(930780);
    REQUIRE(clip != nullptr);
    REQUIRE(clip->control.mode == conf::battle::ClipControlMode::FreeMove);
    REQUIRE(clip->displacementId == -1);

    auto& transform = world.ecs().get<TransformComponent>(hero);
    transform.towardX = 1;
    transform.vectorX = Fixed::fromInt(1);
    auto& intent = world.ecs().get<IntentComponent>(hero);
    intent.move.mode     = MoveMode::None;
    intent.move.quadrant = ControlQuadrant::DownLeft;
    intent.move.angle    = 0;

    REQUIRE(timeline::rules::enterClip(world, hero, 930780, false, false));
    timeline::rules::applyClipControl(world, hero);
    CHECK(transform.towardX == -1);
    CHECK(world.ecs().get<DisplacementComponent>(hero).velocity.x < Fixed{});

    world.ecs().get<DisplacementComponent>(hero).velocity = {};
    auto& timeline = world.ecs().get<ActionTimelineComponent>(hero);
    timeline.animationEnd = true;
    timeline.clipMs       = world.ecs().singleton<WorldClock>().dtMs;
    timeline::rules::applyClipControl(world, hero);
    CHECK(world.ecs().get<DisplacementComponent>(hero).velocity.x < Fixed{});
}

TEST_CASE("attack command with quadrant refreshes sticky stick")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    world.ecs().get<RoleStateComponent>(hero).state = RoleStateId::Attack;
    PlayerCommand command;
    command.controlEvent = ControlEvent::CTR_ATKH;
    command.quadrant     = ControlQuadrant::DownLeft;
    command.angle        = 0;
    command.pressType    = 1;
    world.ecs().get<PlayerControlComponent>(hero).commands.push_back(command);
    runPlayerIntent(world);
    const auto& move = world.ecs().get<IntentComponent>(hero).move;
    CHECK(move.mode == MoveMode::None);
    CHECK(move.quadrant == ControlQuadrant::DownLeft);
    CHECK(move.angle == 0);
}

TEST_CASE("ranger 930190 clip 10902 has six FreeMove displacement frames")
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
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kRanger);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());

    const Entity hero = roleByConfig(world, 102);
    REQUIRE(hero.value != 0);
    auto& book = world.ecs().get<SkillBookComponent>(hero);
    int32_t index = kNoSkill;
    for (int32_t i = 0; i < static_cast<int32_t>(book.skills.size()); ++i)
        if (book.skills[static_cast<size_t>(i)].configId == 930190)
            index = i;
    REQUIRE(index != kNoSkill);

    world.ecs().get<RoleStateComponent>(hero).state = RoleStateId::Attack;
    auto& intent = world.ecs().get<IntentComponent>(hero);
    intent.move.mode     = MoveMode::None;
    intent.move.quadrant = ControlQuadrant::DownLeft;
    intent.move.angle    = 0;
    skill::rules::beginCast(world, hero, index, true);

    Fixed lastX = world.ecs().get<TransformComponent>(hero).position.x;
    int moves   = 0;
    std::string log;
    for (int step = 1; step <= 16; ++step)
    {
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
        const auto& timeline = world.ecs().get<ActionTimelineComponent>(hero);
        const auto& pos      = world.ecs().get<TransformComponent>(hero).position;
        const auto& vel      = world.ecs().get<DisplacementComponent>(hero).velocity;
        const Fixed dx       = pos.x - lastX;
        log += std::to_string(step) + " clip=" + std::to_string(timeline.clipId) +
               " clipMs=" + std::to_string(timeline.clipMs.toInt()) +
               " end=" + (timeline.animationEnd ? "1" : "0") +
               " defer=" + (timeline.deferAdvance ? "1" : "0") +
               " vel=" + std::to_string(vel.x.toInt()) +
               " dx=" + std::to_string(dx.toInt()) + "\n";
        if (timeline.clipId == 930780 && dx < Fixed{})
            ++moves;
        lastX = pos.x;
        intent.move.quadrant = ControlQuadrant::DownLeft;
        intent.move.angle    = 0;
        intent.move.mode     = MoveMode::None;
        world.ecs().get<RoleStateComponent>(hero).state = RoleStateId::Attack;
    }
    INFO(log);
    CHECK(moves == 6);
}

TEST_CASE("disp- uses end-of-frame position after a replacement curve")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    auto& transform      = world.ecs().get<TransformComponent>(hero);
    transform.position = {Fixed::fromInt(100), {}, Fixed::fromInt(250)};
    transform.towardX  = -1;
    transform.vectorX  = "-1"_fx;
    REQUIRE(displacement::rules::set(world, hero, 934660));
    runDisplacement(world);
    displacement::rules::reset(world, hero);
    REQUIRE(displacement::rules::set(world, hero, 922030));
    runDisplacement(world);
    const Fixed liveX = world.ecs().get<TransformComponent>(hero).position.x;
    runDebugSink(world);
    const auto toMilli = [](Fixed value) {
        const int64_t scaled = value.raw * 1000;
        int64_t result       = scaled / Fixed::kOne;
        if (scaled % Fixed::kOne != 0 && scaled < 0)
            --result;
        return result;
    };
    bool found = false;
    for (const auto& line : world.traceLines())
    {
        if (line.find("disp- ") != 0 || line.find(" 934660 ") == std::string::npos)
            continue;
        found = true;
        CHECK(line.find(' ' + std::to_string(toMilli(liveX)) + ' ') != std::string::npos);
    }
    CHECK(found);
}

TEST_CASE("wall policy Pass skips map clamp")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    // bootstrap 出生阶段是 Room，出生帧位移会被跳过；推一帧后再积分。
    world.ecs().singleton<WorldClock>().frame = 1;

    auto& region = world.ecs().singleton<MapRegion>();
    region.moveRanges = {{Fixed::fromInt(33), Fixed::fromInt(200), Fixed::fromInt(400), Fixed::fromInt(100)}};

    auto place = [&] {
        auto& transform      = world.ecs().get<TransformComponent>(hero);
        transform.position = {Fixed::fromInt(80), {}, Fixed::fromInt(250)};
        transform.towardX  = -1;
        transform.vectorX  = "-1"_fx;
        world.ecs().get<HitboxesComponent>(hero).radius = Fixed::fromInt(20);
        REQUIRE(displacement::rules::set(world, hero, 980071));
    };

    place();
    world.ecs().get<DisplacementComponent>(hero).wallPolicy =
        static_cast<int32_t>(conf::battle::ClipWallPolicy::Default);
    for (int i = 0; i < 8; ++i)
        runDisplacement(world);
    const Fixed clamped = world.ecs().get<TransformComponent>(hero).position.x;
    CHECK(clamped == Fixed::fromInt(53));
    CHECK(world.ecs().get<DisplacementComponent>(hero).wall);

    place();
    world.ecs().get<DisplacementComponent>(hero).wallPolicy =
        static_cast<int32_t>(conf::battle::ClipWallPolicy::Pass);
    for (int i = 0; i < 8; ++i)
        runDisplacement(world);
    const auto& passDisp = world.ecs().get<DisplacementComponent>(hero);
    CHECK(world.ecs().get<TransformComponent>(hero).position.x < Fixed::fromInt(53));
    CHECK_FALSE(passDisp.wall);
}
