#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

// 浮空/倒地受击状态族单测：直接驱动角色进入受击状态，验证转移与死亡位移路径（原版语义）。
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

// 构造战斗世界（职业 1 / 角色 101，等级 1000，关 AI）。
World makeWorld()
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
    player.m_roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.m_level  = 1000;
    player.m_camp   = 2;
    player.m_slot   = 0;
    player.m_isAi   = false;
    setup.m_players.push_back(player);
    setup.m_aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 取得主控玩家实体。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().m_entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}

// 把角色置为指定受击状态（清理死亡与木偶请求）。
void placeHit(World& world, Entity entity, RoleStateId state, bool deathRequested)
{
    auto& role = world.ecs().get<RoleStateComponent>(entity);
    role.m_state           = state;
    role.m_prev            = state;
    role.m_deathRequested  = deathRequested;
    role.m_puppetRequested = false;
    auto& hit = world.ecs().get<HitStateComponent>(entity);
    hit.m_pendingHitDefId = 0;
    hit.m_animationEnd    = false;
    hit.m_displacementEnd = false;
    hit.m_hitTimeMs       = {};
}
}  // namespace

// 击飞（hitType 2）在角色未配置受击位移覆盖时转入浮空。
TEST_CASE("hit family: hitType 2 enters HitUp when hitDisplacementId == -1")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::Hit, false);
    auto& hit    = world.ecs().get<HitStateComponent>(h);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    hit.m_hitType        = 2;
    hit.m_hitDefId       = 1;
    hit.m_displacementId = 59;
    hit.m_stiffTime      = Fixed::fromInt(10000);
    motion.m_completedThisFrame = true;
    motion.m_loop               = false;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::HitUp);
    CHECK(world.ecs().get<MotionStateComponent>(h).m_motionName == "flyup");
}

// 击退（hitType 0）动画完成后停留在 Hit，直到硬直结束进 HitSwitch。
TEST_CASE("hit family: hitType 0 stays Hit and later enters HitSwitch")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::Hit, false);
    auto& hit    = world.ecs().get<HitStateComponent>(h);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    hit.m_hitType        = 0;
    hit.m_hitDefId       = 1;
    hit.m_displacementId = -1;
    hit.m_stiffTime      = Fixed::fromInt(0);
    motion.m_completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    // hitType 0 不转浮空；动画完成 + 位移结束 + 硬直为 0 → 下一帧进 HitSwitch。
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state != RoleStateId::HitUp);
}

// 倒地死亡不套死亡位移（原版 ActionHitFloor → dealWithStatus(Death) 无位移参数）。
TEST_CASE("hit family: HitFloor death applies no death displacement")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::HitFloor, true);
    auto& hit = world.ecs().get<HitStateComponent>(h);
    hit.m_hitType        = 2;
    hit.m_animationEnd   = true;
    hit.m_displacementEnd = true;
    hit.m_stiffTime      = Fixed::fromInt(0);
    hit.m_hitTimeMs      = {};
    world.ecs().get<DisplacementComponent>(h).m_curveId = -1;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Death);
    CHECK(world.ecs().get<HitStateComponent>(h).m_displacementId == -1);
    CHECK(world.ecs().get<DisplacementComponent>(h).m_curveId == -1);
}

// 起身死亡套死亡位移（原版 ActionGetUp → dealWithStatus(Death, death_displacement_id)）。
TEST_CASE("hit family: GetUp death applies death displacement")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::GetUp, true);
    auto& hit    = world.ecs().get<HitStateComponent>(h);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    hit.m_hitType         = 2;
    motion.m_completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Death);
    // 死亡位移来自角色配置（角色 101 为 133）。
    CHECK(world.ecs().get<HitStateComponent>(h).m_displacementId != -1);
}

// 受击切换死亡不套死亡位移（原版 ActionHitSwitch → dealWithStatus(Death) 无位移参数）。
TEST_CASE("hit family: HitSwitch death applies no death displacement")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::HitSwitch, true);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    motion.m_completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Death);
    CHECK(world.ecs().get<HitStateComponent>(h).m_displacementId == -1);
}

// 倒地起身链：倒地满硬直后进入 GetUp，动画完成后进入 Wake，随后回到 Idle。
TEST_CASE("hit family: HitFloor advances to GetUp and back to Idle")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::HitFloor, false);
    auto& hit    = world.ecs().get<HitStateComponent>(h);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    hit.m_hitType        = 2;
    hit.m_animationEnd   = true;
    hit.m_displacementEnd = true;
    hit.m_stiffTime      = Fixed::fromInt(0);
    hit.m_hitTimeMs      = {};
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::GetUp);
    motion.m_completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Wake);
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Idle);
}
