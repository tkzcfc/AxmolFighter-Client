#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

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

World makeWorld(CharacterClass occupation = CharacterClass::kSwordman)
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
    player.roleId = type_conversions::toRoleConfigId(occupation);
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
        if (identity.configId == configId)
            found = entity;
    });
    return found;
}

FrameInput cmdInput(uint64_t frame, ControlEvent event, uint32_t pressType)
{
    FrameInput input;
    input.frame = frame;
    PlayerCommand command;
    command.controlEvent = event;
    command.quadrant     = ControlQuadrant::None;
    command.pressType    = pressType;
    input.players.push_back({0, {command}});
    return input;
}

FrameInput runInput(uint64_t frame)
{
    FrameInput input;
    input.frame = frame;
    PlayerCommand command;
    command.controlEvent = ControlEvent::CTR_RUN;
    command.quadrant     = ControlQuadrant::DownLeft;
    command.pressType    = 1;
    input.players.push_back({0, {command}});
    return input;
}
}  // namespace

TEST_CASE("skill eventSlot maps JOYB/C/D to mode-2 slots")
{
    CHECK(skill::rules::eventSlot(ControlEvent::CTR_JOYB) == "B_2");
    CHECK(skill::rules::eventSlot(ControlEvent::CTR_JOYC) == "C_2");
    CHECK(skill::rules::eventSlot(ControlEvent::CTR_JOYD) == "D_2");
    CHECK(skill::rules::eventSlot(ControlEvent::CTR_JOY1) == "1+");
    CHECK(skill::rules::eventSlot(ControlEvent::CTR_ATKQ) == "P");
}

TEST_CASE("skill offlineKit swordman has overflow J and rub M")
{
    loadAssets();
    const auto kit = skill::rules::offlineKit(CharacterClass::kSwordman);
    std::vector<std::string> slots;
    slots.reserve(kit.size());
    for (const auto& [slot, id] : kit)
    {
        REQUIRE(id > 0);
        slots.push_back(slot);
    }
    CHECK(std::find(slots.begin(), slots.end(), "A") != slots.end());
    CHECK(std::find(slots.begin(), slots.end(), "Y") != slots.end());
    CHECK(std::find(slots.begin(), slots.end(), "I") != slots.end());
    CHECK(std::find(slots.begin(), slots.end(), "J") != slots.end());
    CHECK(std::find(slots.begin(), slots.end(), "M") != slots.end());
}

TEST_CASE("skill dealButton caches a different-slot next skill during sprint")
{
    World world      = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    for (uint64_t frame = 1; frame <= 30; ++frame)
        world.tick(runInput(frame));
    world.tick(cmdInput(31, ControlEvent::CTR_ATKA, 1));
    world.tick(cmdInput(32, ControlEvent::CTR_ATKA, 2));
    const auto& book = world.ecs().get<SkillBookComponent>(hero);
    const auto& cast = world.ecs().get<SkillCastComponent>(hero);
    REQUIRE(cast.skill != kNoSkill);
    CHECK(book.slots[static_cast<size_t>(book.skills[static_cast<size_t>(cast.skill)].slot)] == "Y");

    world.tick(cmdInput(33, ControlEvent::CTR_ATKA, 1));
    const auto& after = world.ecs().get<SkillCastComponent>(hero);
    CHECK(after.nextSkill != kNoSkill);
    const auto& next = book.skills[static_cast<size_t>(after.nextSkill)];
    CHECK(book.slots[static_cast<size_t>(next.slot)] == "A");
}

TEST_CASE("skill getFightSkill advances to the next link when charges run out")
{
    World world      = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    REQUIRE_FALSE(book.slots.empty());
    REQUIRE_FALSE(book.skills.empty());

    const int32_t slotIndex = book.skills.front().slot;
    const std::string slot  = book.slots[static_cast<size_t>(slotIndex)];
    SkillRuntime extra      = book.skills.front();
    extra.link            = 2;
    extra.step            = 1;
    extra.stepCount       = 1;
    extra.mode            = 1;
    extra.configId        = 424242;
    extra.finally         = true;
    book.skills.front().finally   = false;
    book.skills.front().stepCount = 1;
    book.skills.front().releaseMax   = 1;
    book.skills.front().releaseCount = 0;
    book.skills.push_back(extra);

    auto& cast   = world.ecs().get<SkillCastComponent>(hero);
    cast.skill = 0;
    CHECK(skill::rules::getFightSkill(world, hero, slot) == static_cast<int32_t>(book.skills.size() - 1));
}

TEST_CASE("skill sprint cancel reaches A combo head")
{
    World world      = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    bool sawAHead     = false;
    int32_t sawY      = 0;
    for (uint64_t frame = 1; frame <= 30; ++frame)
        world.tick(runInput(frame));
    world.tick(cmdInput(31, ControlEvent::CTR_ATKA, 1));
    world.tick(cmdInput(32, ControlEvent::CTR_ATKA, 2));
    for (uint64_t frame = 33; frame <= 80; ++frame)
    {
        if (frame == 40 || frame == 45 || frame == 50 || frame == 55)
            world.tick(cmdInput(frame, ControlEvent::CTR_ATKA, 1));
        else if (frame == 41 || frame == 46 || frame == 51 || frame == 56)
            world.tick(cmdInput(frame, ControlEvent::CTR_ATKA, 2));
        else
            world.tick(FrameInput{frame, {}, {}, {}});
        const auto& cast = world.ecs().get<SkillCastComponent>(hero);
        if (cast.skill == kNoSkill)
            continue;
        const auto& book = world.ecs().get<SkillBookComponent>(hero);
        const auto& skill = book.skills[static_cast<size_t>(cast.skill)];
        const auto& slot  = book.slots[static_cast<size_t>(skill.slot)];
        if (slot == "Y")
            sawY = skill.configId;
        if (slot == "A" && skill.configId == 920000)
            sawAHead = true;
    }
    CHECK(sawY == 920090);
    CHECK(sawAHead);
}
