#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/replay/Replay.h"
#include "mugen/sim/view/WorldView.h"
#include "mugen/sim/view/HudView.h"
#include "mugen/sim/view/DebugView.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/RoleDef.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

using namespace mugen;
using namespace mugen::sim;
using namespace mugen::sim::view;

namespace
{
// 挂载 VFS 并加载配置（幂等）。
void loadAssets()
{
    REQUIRE(test::ensureContentVfs());
    auto& config = conf::Config::instance();
    if (!config.loaded())
        REQUIRE(config.load("/mugen/config/config.bin"));
}

// 把本帧按键状态推进为一帧逻辑。
void step(World& world, ControlManagerCore& control)
{
    std::vector<PlayerCommand> commands;
    control.advance(commands);
    FrameInput input;
    input.frame = world.frame() + 1;
    if (!commands.empty())
        input.players.push_back({0, std::move(commands)});
    world.tick(input);
}
}  // namespace

TEST_CASE("ControlManager combo emits joystick event")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    for (ControlKey key : {ControlKey::Down, ControlKey::Left, ControlKey::Up, ControlKey::Right, ControlKey::Down})
    {
        control.setKey(key, true);
        control.advance(commands);
        control.setKey(key, false);
        control.advance(commands);
    }
    control.setKey(ControlKey::AttackA, true);
    control.advance(commands);
    REQUIRE(!commands.empty());
    CHECK(commands.back().controlEvent == ControlEvent::CTR_JOY1);
}

TEST_CASE("ControlManager direction after a finished 5-step combo stays in range")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    for (ControlKey key : {ControlKey::Down, ControlKey::Left, ControlKey::Up, ControlKey::Right, ControlKey::Down})
    {
        control.setKey(key, true);
        control.advance(commands);
        control.setKey(key, false);
        control.advance(commands);
    }
    commands.clear();
    control.setKey(ControlKey::Left, true);
    control.advance(commands);
    REQUIRE(!commands.empty());
    CHECK(commands.back().controlEvent == ControlEvent::CTR_RUN);
}

TEST_CASE("ControlManager rapid key toggles after combo stay in range")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    for (ControlKey key : {ControlKey::Down, ControlKey::Left, ControlKey::Up, ControlKey::Right, ControlKey::Down})
    {
        control.setKey(key, true);
        control.advance(commands);
        control.setKey(key, false);
        control.advance(commands);
    }
    for (int i = 0; i < 8; ++i)
    {
        control.setKey(ControlKey::Up, true);
        control.advance(commands);
        control.setKey(ControlKey::Up, false);
        control.advance(commands);
        control.setKey(ControlKey::Down, true);
        control.advance(commands);
        control.setKey(ControlKey::Down, false);
        control.advance(commands);
    }
    REQUIRE(!commands.empty());
    CHECK(commands.back().controlEvent != ControlEvent::CTR_NONE);
}

TEST_CASE("Replay v5 save and load roundtrip")
{
    Replay replay;
    replay.configSchemaHash = 42;
    replay.setup.seed       = 7;
    replay.setup.mode       = WorldMode::Town;
    replay.setup.mapKey     = "city_newcity_xueyuan";
    FrameInput frame;
    frame.frame = 1;
    frame.players.push_back({0, {{ControlEvent::CTR_WALK, ControlQuadrant::UpRight, 90000, 2}}});
    replay.frames.push_back(frame);
    replay.finalWorldHash = 99;

    ByteBuffer bytes;
    replay.serialize(bytes);
    REQUIRE(bytes.len() >= 4);
    CHECK(std::memcmp(bytes.data(), "MGRP", 4) == 0);

    std::string error;
    Replay decoded;
    bytes.resetPosition();
    REQUIRE(decoded.deserialize(bytes, 42, error));
    CHECK(decoded.configSchemaHash == 42);
    CHECK(decoded.setup.seed == 7);
    CHECK(decoded.setup.mode == WorldMode::Town);
    CHECK(decoded.frames.size() == 1);
    CHECK(decoded.finalWorldHash == 99);

    const auto path = std::filesystem::temp_directory_path() / "axmol_replay_v5_test.mgrp";
    REQUIRE(replay.save(path.string(), error));
    Replay loaded;
    REQUIRE(loaded.load(path.string(), 42, error));
    CHECK(loaded.finalWorldHash == 99);
    CHECK(loaded.frames.front().frame == 1);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("Town spawn walk puppet visual entities and destroy")
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed = 1;
    setup.mode = WorldMode::Town;
    world.init(setup);
    world.ecs().singleton<MapRegion>().moveRanges = {{"-10000"_fx, "-10000"_fx, "20000"_fx, "20000"_fx}};

    const Entity hero =
        spawnTownRole(world, type_conversions::toRoleConfigId(CharacterClass::kSwordman), 1, {"10"_fx, {}, {}}, 1, 0);
    REQUIRE(world.ecs().alive(hero));
    CHECK(world.ecs().has<PlayerControlComponent>(hero));

    ControlManagerCore control;
    control.setKey(ControlKey::Right, true);
    const Fixed startX = world.ecs().get<TransformComponent>(hero).position.x;
    for (int i = 0; i < 6; ++i)
        step(world, control);
    const auto moved = readTownMovement(world, hero);
    CHECK(moved.valid);
    CHECK(moved.position.x > startX);
    CHECK(moved.moving);

    const Entity puppet = spawnTownPuppet(world, 1001, type_conversions::toRoleConfigId(CharacterClass::kSwordman),
                                         {"100"_fx, {}, "20"_fx}, -1);
    REQUIRE(world.ecs().alive(puppet));
    FrameInput puppetInput;
    puppetInput.frame = world.frame() + 1;
    PuppetUpdate update;
    update.entity     = puppet;
    update.position   = {"120"_fx, {}, "25"_fx};
    update.towardX    = -1;
    update.motionName = "walk";
    update.status     = EntityRoleStatus::Run | EntityRoleStatus::Idle;
    puppetInput.puppetUpdates.push_back(update);
    world.tick(puppetInput);
    const auto puppetMove = readTownMovement(world, puppet);
    CHECK(puppetMove.valid);
    CHECK(puppetMove.position.x == "120"_fx);
    CHECK(puppetMove.towardX == -1);

    std::vector<VisualEntityView> visuals;
    collectVisualEntities(world, visuals);
    CHECK(visuals.size() >= 2);
    bool foundHero = false;
    bool foundPuppet = false;
    for (const auto& visual : visuals)
    {
        if (visual.entity == hero)
            foundHero = true;
        if (visual.entity == puppet)
            foundPuppet = true;
        CHECK(visual.kind == VisualEntityKind::Role);
    }
    CHECK(foundHero);
    CHECK(foundPuppet);

    destroyTownEntity(world, hero);
    CHECK_FALSE(world.ecs().alive(hero));
    CHECK(world.ecs().singleton<PlayerSlots>().entities.find(0) ==
          world.ecs().singleton<PlayerSlots>().entities.end());
}

TEST_CASE("HudView kit cooldown buffs and battle room combo")
{
    loadAssets();
    const auto rows = offlineKitRows(CharacterClass::kSwordman);
    REQUIRE_FALSE(rows.empty());
    bool foundAttack = false;
    for (const auto& row : rows)
    {
        if (row.slot == EnumSkillSlot::Attack)
        {
            foundAttack = true;
            CHECK(row.slotName == "Attack");
            CHECK(row.poolSlot == "A");
            CHECK(row.hasControlKey);
            CHECK(row.skillId > 0);
        }
    }
    CHECK(foundAttack);
    CHECK_FALSE(offlineKit(CharacterClass::kSwordman).empty());

    World world;
    WorldSetup setup;
    setup.seed = 2;
    setup.mode = WorldMode::Town;
    world.init(setup);
    const Entity player =
        spawnTownRole(world, type_conversions::toRoleConfigId(CharacterClass::kSwordman), 1, {}, 1, 0);
    REQUIRE(world.ecs().alive(player));

    const auto hud = readPlayerHud(world, player);
    CHECK(hud.valid);
    CHECK(hud.roleId == type_conversions::toRoleConfigId(CharacterClass::kSwordman));
    CHECK(hud.maxHp > Fixed{});

    const auto cd = readSlotCooldown(world, player, "A");
    CHECK_FALSE(cd.cooling);

    std::vector<ActiveBuffView> buffs;
    collectActiveBuffs(world, player, buffs);
    if (const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(hud.roleId))
        if (!role->buffIds.empty() && role->buffIds.front() > 0)
        {
            buff::rules::admit(world, player, role->buffIds.front());
            collectActiveBuffs(world, player, buffs);
            CHECK_FALSE(buffs.empty());
        }

    auto& combo      = world.ecs().get<ComboMeterComponent>(player);
    combo.maxCount = 7;
    combo.maxGrade = 3;
    const auto room  = readBattleRoom(world, player);
    CHECK_FALSE(room.enabled);
    CHECK(room.maxCombo == 7);
    CHECK(room.maxEvaluate == 3);
    CHECK(room.pendingEmpty);
    CHECK(room.state == mugen::RoomState::None);
}

TEST_CASE("DebugView colliders role debug and role effect spines")
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed = 3;
    setup.mode = WorldMode::Town;
    world.init(setup);
    const Entity player =
        spawnTownRole(world, type_conversions::toRoleConfigId(CharacterClass::kSwordman), 1, {"5"_fx, {}, {}}, 1, 0);
    REQUIRE(playerEntity(world, 0) == player);

    std::vector<ColliderDebugView> colliders;
    collectColliders(world, colliders);
    REQUIRE_FALSE(colliders.empty());
    CHECK(colliders.front().entity == player);

    std::vector<RoleDebugView> roles;
    collectRoleDebug(world, roles);
    REQUIRE_FALSE(roles.empty());
    CHECK(roles.front().roleId == type_conversions::toRoleConfigId(CharacterClass::kSwordman));
    CHECK(roles.front().position.x == "5"_fx);

    const auto ids = collectRoleEffectSpineIds(101);
    REQUIRE_FALSE(ids.empty());
    CHECK(std::is_sorted(ids.begin(), ids.end()));
    CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
    for (const int32_t id : ids)
        CHECK(conf::Config::instance().find<conf::ResSpineConfig>(id) != nullptr);
    CHECK(collectRoleEffectSpineIds(0).empty());
}
