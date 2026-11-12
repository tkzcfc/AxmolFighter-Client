#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/skill/SkillRules.h"
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
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
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
    const auto& heroPos = world.ecs().get<TransformComponent>(hero).m_position;
    CHECK(world.ecs().get<IdentityComponent>(hero).m_level == 1000);
    CHECK(heroPos.x == Fixed::fromInt(2037));
    CHECK(heroPos.z == Fixed::fromInt(254));
    CHECK(world.ecs().get<TransformComponent>(hero).m_towardX == -1);
    CHECK(world.ecs().get<HealthComponent>(hero).m_hp.toInt() == 413387979);
    CHECK(world.ecs().get<HealthComponent>(hero).m_maxHp.toInt() == 413387979);

    // 怪物 61300 / 61320：位置 (1006/1186, 0, 250)，生命 12101080。
    const auto& p0 = world.ecs().get<TransformComponent>(m0).m_position;
    const auto& p1 = world.ecs().get<TransformComponent>(m1).m_position;
    CHECK(p0.x == Fixed::fromInt(1006));
    CHECK(p0.z == Fixed::fromInt(250));
    CHECK(p1.x == Fixed::fromInt(1186));
    CHECK(p1.z == Fixed::fromInt(250));
    CHECK(world.ecs().get<HealthComponent>(m0).m_hp.toInt() == 12101080);
    CHECK(world.ecs().get<HealthComponent>(m1).m_hp.toInt() == 12101080);
}

TEST_CASE("sim locomotion runs right and returns to idle")
{
    World world = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    const Fixed start = world.ecs().get<TransformComponent>(hero).m_position.x;

    for (uint64_t frame = 1; frame <= 10; ++frame)
        world.tick(runCommand(frame, ControlEvent::CTR_RUN, ControlQuadrant::UpRight));
    CHECK(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Run);
    CHECK(world.ecs().get<MotionStateComponent>(hero).m_motionName == "walk");
    const Fixed afterRun = world.ecs().get<TransformComponent>(hero).m_position.x;
    CHECK(afterRun > start);

    world.tick(runCommand(world.frame() + 1, ControlEvent::CTR_IDLE, ControlQuadrant::None));
    CHECK(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Idle);
    CHECK(world.ecs().get<MotionStateComponent>(hero).m_motionName == "stand");
}

TEST_CASE("sim map clamp pins left boundary for room 2071501")
{
    World world = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    // 持续向左跑，直到撞上左边界（33.5 + 半径 20 = 53.5）。
    for (uint64_t frame = 1; frame <= 200; ++frame)
        world.tick(runCommand(frame, ControlEvent::CTR_RUN, ControlQuadrant::DownLeft));
    const Fixed x = world.ecs().get<TransformComponent>(hero).m_position.x;
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
        dmgMin = baseHp - world.ecs().get<HealthComponent>(dummy).m_hp.toInt();
    }
    {
        World world = makeWorld();
        DrawPolicy policy;
        policy.m_crit = Fixed::fromInt(0);  // 必暴击。
        world.setDrawPolicy(&policy);
        const Entity dummy = roleByConfig(world, 61300);
        driveCombo(world, 58);
        dmgMax = baseHp - world.ecs().get<HealthComponent>(dummy).m_hp.toInt();
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
    CHECK(world.ecs().get<PauseComponent>(hero).m_freezeMs == Fixed::fromInt(20));
    // 下一帧顿帧被 PauseTick 消耗。
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<PauseComponent>(hero).m_freezeMs == Fixed{});
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
        const auto& cast = world.ecs().get<SkillCastComponent>(hero);
        if (cast.m_skill != kNoSkill)
        {
            const auto& book = world.ecs().get<SkillBookComponent>(hero);
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
    const auto& flags  = world.ecs().get<StatusFlagsComponent>(dummy);
    CHECK_FALSE(flags.active(ExtraState::SuperArmor));
    buff::rules::admit(world, dummy, 101);
    buff::rules::admit(world, dummy, 102);
    CHECK(flags.active(ExtraState::SuperArmor));
    CHECK(flags.m_refs[static_cast<size_t>(ExtraState::SuperArmor) - 1] == 2);
    CHECK(world.ecs().get<BuffListComponent>(dummy).m_instances.size() == 2);
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
    CHECK(world.ecs().get<BuffListComponent>(dummy).m_instances.empty());
}

TEST_CASE("sim buff: super armored target takes damage but never enters Hit")
{
    World world        = makeWorld();
    const Entity dummy = roleByConfig(world, 61300);
    buff::rules::admit(world, dummy, 101);
    const Fixed hpBefore = world.ecs().get<HealthComponent>(dummy).m_hp;
    driveCombo(world, 70);
    CHECK(world.ecs().get<HealthComponent>(dummy).m_hp < hpBefore);
    CHECK(world.ecs().get<RoleStateComponent>(dummy).m_state == RoleStateId::Idle);
    CHECK(world.ecs().get<HitStateComponent>(dummy).m_pendingHitDefId == 0);
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
    CHECK(restored.ecs().get<BuffListComponent>(restoredDummy).m_instances.size() == 1);
    CHECK(restored.ecs().get<StatusFlagsComponent>(restoredDummy).active(ExtraState::SuperArmor));
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
        CHECK_FALSE(world.ecs().get<StatusFlagsComponent>(hero).active(ExtraState::SuperArmor));
        CHECK(world.ecs().get<BuffListComponent>(hero).m_instances.empty());
        hashes[run] = world.hash();
    }
    CHECK(hashes[0] == hashes[1]);
}

TEST_CASE("sim hit: two separate hit cycles reset hit timers")
{
    World world        = makeWorld();
    const Entity dummy = roleByConfig(world, 61300);
    const auto inputs  = aComboInputs();
    int cycles         = 0;
    RoleStateId prev   = RoleStateId::Idle;
    for (uint64_t f = 1; f <= 140; ++f)
    {
        const auto it = inputs.find(f);
        world.tick(it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second);
        const auto state = world.ecs().get<RoleStateComponent>(dummy).m_state;
        if (state == RoleStateId::Hit && prev != RoleStateId::Hit)
        {
            const auto& hit = world.ecs().get<HitStateComponent>(dummy);
            CHECK_FALSE(hit.m_animationEnd);
            CHECK(hit.m_hitTimeMs == Fixed{});
            ++cycles;
        }
        prev = state;
    }
    CHECK(cycles >= 2);
}

TEST_CASE("sim skill: cast rejected while in Hit")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    world.ecs().get<RoleStateComponent>(hero).m_state = RoleStateId::Hit;
    auto& hit = world.ecs().get<HitStateComponent>(hero);
    hit.m_stiffTime       = Fixed::fromInt(5000);
    hit.m_hitTimeMs       = {};
    hit.m_animationEnd    = false;
    hit.m_displacementEnd = false;
    world.ecs().get<MotionStateComponent>(hero).m_completedThisFrame = false;
    world.tick(cmdInput(world.frame() + 1, ControlEvent::CTR_ATKA, ControlQuadrant::None, 1));
    CHECK(world.ecs().get<SkillCastComponent>(hero).m_skill == kNoSkill);
    CHECK(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Hit);
}

TEST_CASE("sim timeline stays frozen while Hit")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    driveCombo(world, 52);
    REQUIRE(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Attack);
    const Fixed clipMs = world.ecs().get<ActionTimelineComponent>(hero).m_clipMs;
    REQUIRE(clipMs > Fixed{});
    world.ecs().get<RoleStateComponent>(hero).m_state = RoleStateId::Hit;
    auto& hit = world.ecs().get<HitStateComponent>(hero);
    hit.m_stiffTime       = Fixed::fromInt(5000);
    hit.m_hitTimeMs       = {};
    hit.m_animationEnd    = false;
    hit.m_displacementEnd = false;
    world.ecs().get<MotionStateComponent>(hero).m_completedThisFrame = false;
    world.ecs().get<DisplacementComponent>(hero).m_brake = false;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<ActionTimelineComponent>(hero).m_clipMs == clipMs);
    CHECK(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Hit);
}

TEST_CASE("sim death is requested then entered by the fsm")
{
    World world        = makeWorld();
    const Entity dummy = roleByConfig(world, 61300);
    const Fixed hp     = world.ecs().get<HealthComponent>(dummy).m_hp;
    health::rules::applyDelta(world, dummy, -hp, HpCause::Hit, dummy, {});
    CHECK(world.ecs().get<HealthComponent>(dummy).m_dead);
    CHECK_FALSE(world.ecs().singleton<EventQueue<DeathRequest>>().empty());
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().singleton<EventQueue<DeathRequest>>().empty());
    CHECK(world.ecs().get<RoleStateComponent>(dummy).m_deathRequested);
    CHECK(world.ecs().get<RoleStateComponent>(dummy).m_deathAnnounced);
    CHECK(world.ecs().get<RoleStateComponent>(dummy).m_state != RoleStateId::Death);
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(dummy).m_state == RoleStateId::Death);
    CHECK(world.ecs().get<MotionStateComponent>(dummy).m_motionName == "death");
}

TEST_CASE("sim clip buffs released when a cast is interrupted")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    const auto inputs = superArmorInputs();
    bool armed        = false;
    for (uint64_t f = 1; f <= 40 && !armed; ++f)
    {
        const auto it = inputs.find(f);
        world.tick(it == inputs.end() ? FrameInput{f, {}, {}, {}} : it->second);
        armed = world.ecs().get<StatusFlagsComponent>(hero).active(ExtraState::SuperArmor) &&
                world.ecs().get<SkillCastComponent>(hero).m_skill != kNoSkill;
    }
    REQUIRE(armed);
    const auto armor = static_cast<size_t>(ExtraState::SuperArmor) - 1;
    CHECK(world.ecs().get<StatusFlagsComponent>(hero).m_refs[armor] > 0);
    skill::rules::forceInterrupt(world, hero);
    CHECK(world.ecs().get<SkillCastComponent>(hero).m_skill == kNoSkill);
    CHECK(world.ecs().get<StatusFlagsComponent>(hero).m_refs[armor] == 0);
}

TEST_CASE("sim combat hit interrupts a cast")
{
    World world        = makeWorld();
    const Entity hero  = roleByConfig(world, 101);
    const Entity dummy = roleByConfig(world, 61300);
    driveCombo(world, 52);
    REQUIRE(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Attack);
    REQUIRE(world.ecs().get<SkillCastComponent>(hero).m_skill != kNoSkill);
    effect::rules::spawnFromRole(world, dummy, 920000, {});
    life::rules::spawnFlush(world, SpawnStage::Combat);
    Entity effect;
    world.ecs().each<EffectInfoComponent>([&](Entity entity, const EffectInfoComponent& info) {
        if (info.m_configId == 920000 && info.m_owner == dummy)
            effect = entity;
    });
    REQUIRE(effect.value != 0);
    ContactEvent contact;
    contact.m_a     = effect;
    contact.m_b     = hero;
    contact.m_phase = ContactPhase::Began;
    world.ecs().singleton<EventQueue<ContactEvent>>().push(contact);
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<SkillCastComponent>(hero).m_skill == kNoSkill);
    CHECK(world.ecs().get<SkillCastComponent>(hero).m_interrupted);
}

TEST_CASE("sim rng: identical runs without DrawPolicy match, different seeds can change damage")
{
    auto drive = [](uint32_t seed) {
        World world = makeWorld();
        world.ecs().singleton<RngState>().m_random.seed(seed);
        const Entity dummy = roleByConfig(world, 61300);
        driveCombo(world, 58);
        return std::pair{world.hash(), world.ecs().get<HealthComponent>(dummy).m_hp.toInt()};
    };
    const auto first  = drive(31);
    const auto second = drive(31);
    CHECK(first.first == second.first);
    CHECK(first.second == second.second);
    bool damageDiffers = false;
    for (uint32_t seed = 1; seed <= 16; ++seed)
        if (drive(seed).second != first.second)
            damageDiffers = true;
    CHECK(damageDiffers);
}
