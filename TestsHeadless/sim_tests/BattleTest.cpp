#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <algorithm>
#include <map>

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

// 构建 class1_move 场景世界：职业 1 / 角色 101，等级 1000，房间 2071501。
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
    player.m_class  = CharacterClass::kSwordman;
    player.m_roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.m_level  = 1000;
    player.m_camp   = 2;
    player.m_slot   = 0;
    player.m_isAi   = false;
    setup.m_players.push_back(player);
    setup.m_stageId  = 0;
    setup.m_copyId   = 0;
    setup.m_aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 构造单条跑动命令输入。
FrameInput runCommand(uint64_t frame, ControlEvent event, ControlQuadrant quadrant, int32_t angle = 0)
{
    FrameInput input;
    input.frame = frame;
    PlayerCommand command;
    command.controlEvent = event;
    command.quadrant     = quadrant;
    command.angle        = angle;
    command.pressType    = 1;
    input.players.push_back({0, {command}});
    return input;
}

// 按配置 id 查找角色实体。
Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<Identity>([&](Entity entity, const Identity& identity) {
        if (identity.m_configId == configId)
            found = entity;
    });
    return found;
}

// 构造单条命令输入（可指定按下类型）。
FrameInput cmdInput(uint64_t frame, ControlEvent event, ControlQuadrant quadrant, uint32_t pressType, int32_t angle = 0)
{
    FrameInput input;
    input.frame = frame;
    PlayerCommand command;
    command.controlEvent = event;
    command.quadrant     = quadrant;
    command.angle        = angle;
    command.pressType    = pressType;
    input.players.push_back({0, {command}});
    return input;
}

// A 连招输入：向左跑入、停止、连续按 A 键驱动 920000-920040（与 class1_a_dummy 脚本一致）。
std::map<uint64_t, FrameInput> aComboInputs()
{
    std::map<uint64_t, FrameInput> inputs;
    for (uint64_t f = 5; f <= 44; ++f)
        inputs[f] = cmdInput(f, ControlEvent::CTR_RUN, ControlQuadrant::DownLeft, 1);
    inputs[46] = cmdInput(46, ControlEvent::CTR_IDLE, ControlQuadrant::None, 0);
    for (uint64_t f = 50; f <= 118; f += 4)
    {
        inputs[f]     = cmdInput(f, ControlEvent::CTR_ATKA, ControlQuadrant::None, 1);
        inputs[f + 1] = cmdInput(f + 1, ControlEvent::CTR_ATKA, ControlQuadrant::None, 2);
    }
    return inputs;
}

// 按 A 连招输入推进世界到指定帧（无脚本命令的帧给空输入）。
void driveCombo(World& world, uint64_t frames)
{
    const auto inputs = aComboInputs();
    for (uint64_t f = 1; f <= frames; ++f)
    {
        const auto it = inputs.find(f);
        world.tick(it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second);
    }
}
}  // namespace

TEST_CASE("sim spawn positions and stats match golden for room 2071501")
{
    World world = makeWorld();
    const Entity hero  = roleByConfig(world, 101);
    const Entity m0    = roleByConfig(world, 61300);
    const Entity m1    = roleByConfig(world, 61320);
    REQUIRE(hero.value != 0);
    REQUIRE(m0.value != 0);
    REQUIRE(m1.value != 0);

    // 角色 101：等级 1000，出生点 (2037,0,254)（地图 actor_pos），面向左，生命 413387979。
    const auto& heroPos = world.ecs().get<Transform>(hero).m_position;
    CHECK(world.ecs().get<Identity>(hero).m_level == 1000);
    CHECK(heroPos.x == Fixed::fromInt(2037));
    CHECK(heroPos.z == Fixed::fromInt(254));
    CHECK(world.ecs().get<Transform>(hero).m_towardX == -1);
    CHECK(world.ecs().get<Health>(hero).m_hp.toInt() == 413387979);
    CHECK(world.ecs().get<Health>(hero).m_maxHp.toInt() == 413387979);

    // 怪物 61300 / 61320：位置 (1006/1186, 0, 250)，生命 12101080。
    const auto& p0 = world.ecs().get<Transform>(m0).m_position;
    const auto& p1 = world.ecs().get<Transform>(m1).m_position;
    CHECK(p0.x == Fixed::fromInt(1006));
    CHECK(p0.z == Fixed::fromInt(250));
    CHECK(p1.x == Fixed::fromInt(1186));
    CHECK(p1.z == Fixed::fromInt(250));
    CHECK(world.ecs().get<Health>(m0).m_hp.toInt() == 12101080);
    CHECK(world.ecs().get<Health>(m1).m_hp.toInt() == 12101080);
}

TEST_CASE("sim locomotion runs right and returns to idle")
{
    World world = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    const Fixed start = world.ecs().get<Transform>(hero).m_position.x;

    for (uint64_t frame = 1; frame <= 10; ++frame)
        world.tick(runCommand(frame, ControlEvent::CTR_RUN, ControlQuadrant::UpRight));
    CHECK(world.ecs().get<RoleState>(hero).m_state == RoleStateId::Run);
    CHECK(world.ecs().get<MotionState>(hero).m_motionName == "walk");
    const Fixed afterRun = world.ecs().get<Transform>(hero).m_position.x;
    CHECK(afterRun > start);

    world.tick(runCommand(world.frame() + 1, ControlEvent::CTR_IDLE, ControlQuadrant::None));
    CHECK(world.ecs().get<RoleState>(hero).m_state == RoleStateId::Idle);
    CHECK(world.ecs().get<MotionState>(hero).m_motionName == "stand");
}

TEST_CASE("sim map clamp pins left boundary for room 2071501")
{
    World world = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    // 持续向左跑，直到撞上左边界（33.5 + 半径 20 = 53.5）。
    for (uint64_t frame = 1; frame <= 200; ++frame)
        world.tick(runCommand(frame, ControlEvent::CTR_RUN, ControlQuadrant::DownLeft));
    const Fixed x = world.ecs().get<Transform>(hero).m_position.x;
    CHECK(x == "53.5"_fx);
}

TEST_CASE("sim determinism: same input twice yields same hash")
{
    World a = makeWorld();
    World b = makeWorld();
    for (uint64_t frame = 1; frame <= 40; ++frame)
    {
        const auto input = runCommand(frame, frame % 10 == 0 ? ControlEvent::CTR_IDLE : ControlEvent::CTR_RUN,
                                       ControlQuadrant::DownLeft, frame % 5 == 0 ? 45000 : 0);
        a.tick(input);
        b.tick(input);
        CHECK(a.hash() == b.hash());
    }
}

TEST_CASE("sim snapshot round trip mid-run stays equal")
{
    World world = makeWorld();
    for (uint64_t frame = 1; frame <= 30; ++frame)
        world.tick(runCommand(frame, ControlEvent::CTR_RUN, ControlQuadrant::DownLeft));
    ByteBuffer buffer;
    world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    CHECK(restored.hash() == world.hash());
    for (uint64_t frame = 31; frame <= 50; ++frame)
    {
        const auto input = runCommand(frame, ControlEvent::CTR_RUN, ControlQuadrant::DownLeft);
        world.tick(input);
        restored.tick(input);
        CHECK(restored.hash() == world.hash());
    }
}

TEST_CASE("sim combat damage equality at crit min and max")
{
    const int64_t baseHp = 12101080;
    int64_t dmgMin       = 0;
    int64_t dmgMax       = 0;
    {
        World world = makeWorld();
        DrawPolicy policy;
        policy.m_crit = Fixed::fromInt(101);  // 永不暴击。
        world.setDrawPolicy(&policy);
        const Entity dummy = roleByConfig(world, 61300);
        driveCombo(world, 58);
        dmgMin = baseHp - world.ecs().get<Health>(dummy).m_hp.toInt();
    }
    {
        World world = makeWorld();
        DrawPolicy policy;
        policy.m_crit = Fixed::fromInt(0);  // 必暴击。
        world.setDrawPolicy(&policy);
        const Entity dummy = roleByConfig(world, 61300);
        driveCombo(world, 58);
        dmgMax = baseHp - world.ecs().get<Health>(dummy).m_hp.toInt();
    }
    CHECK(dmgMin == 1558668);
    CHECK(dmgMax == 2338002);
    // 暴击倍率 1.5：max = min * 1.5（整数精确）。
    CHECK(dmgMax == dmgMin * 3 / 2);
}

TEST_CASE("sim combat hitstop freezes attacker for one frame")
{
    World world      = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    driveCombo(world, 58);
    // 命中后攻击者（英雄）顿帧 20ms。
    CHECK(world.ecs().get<Pause>(hero).m_freezeMs == Fixed::fromInt(20));
    // 下一帧顿帧被 PauseTick 消耗。
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<Pause>(hero).m_freezeMs == Fixed{});
}

TEST_CASE("sim combat A-combo cancel chaining reaches final skill")
{
    World world      = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    int32_t maxSkill  = 0;
    const auto inputs = aComboInputs();
    for (uint64_t f = 1; f <= 260; ++f)
    {
        const auto it = inputs.find(f);
        world.tick(it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second);
        const auto& cast = world.ecs().get<SkillCast>(hero);
        if (cast.m_skill != kNoSkill)
        {
            const auto& book = world.ecs().get<SkillBook>(hero);
            maxSkill         = std::max(maxSkill, book.m_skills[static_cast<size_t>(cast.m_skill)].m_configId);
        }
    }
    // 连招应推进到末段技能 920040。
    CHECK(maxSkill == 920040);
}

TEST_CASE("sim combat determinism: same A-combo twice yields same hash")
{
    World a = makeWorld();
    World b = makeWorld();
    const auto inputs = aComboInputs();
    for (uint64_t f = 1; f <= 80; ++f)
    {
        const auto it = inputs.find(f);
        const auto input = it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second;
        a.tick(input);
        b.tick(input);
        CHECK(a.hash() == b.hash());
    }
}

TEST_CASE("sim combat snapshot round trip at frame 80 continues identically")
{
    World world      = makeWorld();
    const auto inputs = aComboInputs();
    for (uint64_t f = 1; f <= 80; ++f)
    {
        const auto it = inputs.find(f);
        world.tick(it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second);
    }
    ByteBuffer buffer;
    world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    CHECK(restored.hash() == world.hash());
    for (uint64_t f = 81; f <= 130; ++f)
    {
        const auto it = inputs.find(f);
        const auto input = it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second;
        world.tick(input);
        restored.tick(input);
        CHECK(restored.hash() == world.hash());
    }
}

TEST_CASE("sim buff: super armor refcount, events and hit-state block")
{
    World world        = makeWorld();
    const Entity dummy = roleByConfig(world, 61300);
    const auto& flags  = world.ecs().get<StatusFlags>(dummy);
    CHECK_FALSE(flags.active(ExtraState::SuperArmor));
    buff::rules::admit(world, dummy, 101);
    buff::rules::admit(world, dummy, 102);
    CHECK(flags.active(ExtraState::SuperArmor));
    CHECK(flags.m_refs[static_cast<size_t>(ExtraState::SuperArmor) - 1] == 2);
    CHECK(world.ecs().get<BuffList>(dummy).m_instances.size() == 2);
    // 移除一个后仍有一层引用。
    buff::rules::remove(world, dummy, 101);
    CHECK(flags.m_refs[static_cast<size_t>(ExtraState::SuperArmor) - 1] == 1);
    buff::rules::remove(world, dummy, 102);
    CHECK_FALSE(flags.active(ExtraState::SuperArmor));
    // 重复移除不会让引用计数变负或二次撤销。
    buff::rules::remove(world, dummy, 102);
    CHECK(flags.m_refs[static_cast<size_t>(ExtraState::SuperArmor) - 1] == 0);
    // BuffTick 下一帧回收实例。
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<BuffList>(dummy).m_instances.empty());
}

TEST_CASE("sim buff: super armored target takes damage but never enters Hit")
{
    World world        = makeWorld();
    const Entity dummy = roleByConfig(world, 61300);
    buff::rules::admit(world, dummy, 101);
    const Fixed hpBefore = world.ecs().get<Health>(dummy).m_hp;
    driveCombo(world, 70);
    CHECK(world.ecs().get<Health>(dummy).m_hp < hpBefore);
    CHECK(world.ecs().get<RoleState>(dummy).m_state == RoleStateId::Idle);
    CHECK(world.ecs().get<HitState>(dummy).m_pendingHitDefId == 0);
}

TEST_CASE("sim buff: snapshot round trip and hash equality with live buff")
{
    World world        = makeWorld();
    const Entity dummy = roleByConfig(world, 61300);
    buff::rules::admit(world, dummy, 101);
    driveCombo(world, 20);
    ByteBuffer buffer;
    world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    CHECK(restored.hash() == world.hash());
    const Entity restoredDummy = roleByConfig(restored, 61300);
    CHECK(restored.ecs().get<BuffList>(restoredDummy).m_instances.size() == 1);
    CHECK(restored.ecs().get<StatusFlags>(restoredDummy).active(ExtraState::SuperArmor));
    const auto inputs = aComboInputs();
    for (uint64_t f = 21; f <= 70; ++f)
    {
        const auto it    = inputs.find(f);
        const auto input = it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second;
        world.tick(input);
        restored.tick(input);
        CHECK(restored.hash() == world.hash());
    }
}

// class1_superarmor 场景输入：向右无移动，直接放 C 槽（脚本事件 102）。
namespace
{
std::map<uint64_t, FrameInput> superArmorInputs()
{
    std::map<uint64_t, FrameInput> inputs;
    inputs[5] = cmdInput(5, ControlEvent::CTR_ATKC, ControlQuadrant::None, 1);
    inputs[6] = cmdInput(6, ControlEvent::CTR_ATKC, ControlQuadrant::None, 2);
    return inputs;
}
}  // namespace

TEST_CASE("sim buff: class1_superarmor hash is stable across two runs and clip buffs clean up")
{
    uint64_t hashes[2] = {0, 0};
    for (int run = 0; run < 2; ++run)
    {
        World world        = makeWorld();
        const Entity hero  = roleByConfig(world, 101);
        const auto inputs  = superArmorInputs();
        for (uint64_t f = 1; f <= 60; ++f)
        {
            const auto it = inputs.find(f);
            world.tick(it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second);
        }
        // 技能结束后片段 Buff 全部移除，霸体引用归零。
        CHECK_FALSE(world.ecs().get<StatusFlags>(hero).active(ExtraState::SuperArmor));
        CHECK(world.ecs().get<BuffList>(hero).m_instances.empty());
        hashes[run] = world.hash();
    }
    CHECK(hashes[0] == hashes[1]);
}
