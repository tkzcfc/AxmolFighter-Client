#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/battle/RoleDef.h"
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
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 取得主控玩家实体。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}

// 把角色置为指定受击状态（清理死亡与木偶请求）。
void placeHit(World& world, Entity entity, RoleStateId state, bool deathRequested)
{
    auto& role = world.ecs().get<RoleStateComponent>(entity);
    role.state           = state;
    role.prev            = state;
    role.deathRequested  = deathRequested;
    role.puppetRequested = false;
    auto& hit = world.ecs().get<HitStateComponent>(entity);
    hit.pendingHitDefId = 0;
    hit.animationEnd    = false;
    hit.displacementEnd = false;
    hit.hitTimeMs       = {};
}

// 按配置 id 查找非玩家槽位角色（取第一个）。
Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == configId && identity.kind == SpawnKind::Role &&
            !identity.playerSlot.has_value() && found.value == 0)
            found = entity;
    });
    return found;
}

// 取房间内的 21881 小怪；若尚未生成则按配置补刷一只。
Entity monster21881(World& world)
{
    Entity monster = roleByConfig(world, 21881);
    if (monster.value != 0)
        return monster;
    RoleSpawnParams params;
    params.roleId = 21881;
    params.level  = 1;
    params.isAi   = false;
    role::rules::spawnRole(world, params);
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    return roleByConfig(world, 21881);
}

// 推进最多 maxFrames 帧，直到实体被回收。
void tickUntilGone(World& world, Entity entity, int maxFrames)
{
    for (int i = 0; i < maxFrames && world.ecs().alive(entity); ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
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
    hit.hitType        = 2;
    hit.hitDefId       = 1;
    hit.displacementId = 59;
    hit.stiffTime      = Fixed::fromInt(10000);
    motion.completedThisFrame = true;
    motion.loop               = false;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::HitUp);
    CHECK(world.ecs().get<MotionStateComponent>(h).motionName == "flyup");
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
    hit.hitType        = 0;
    hit.hitDefId       = 1;
    hit.displacementId = -1;
    hit.stiffTime      = Fixed::fromInt(0);
    motion.completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    // hitType 0 不转浮空；动画完成 + 位移结束 + 硬直为 0 → 下一帧进 HitSwitch。
    CHECK(world.ecs().get<RoleStateComponent>(h).state != RoleStateId::HitUp);
}

// 倒地死亡不套死亡位移（原版 ActionHitFloor → dealWithStatus(Death) 无位移参数）。
TEST_CASE("hit family: HitFloor death applies no death displacement")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::HitFloor, true);
    auto& hit = world.ecs().get<HitStateComponent>(h);
    hit.hitType        = 2;
    hit.animationEnd   = true;
    hit.displacementEnd = true;
    hit.stiffTime      = Fixed::fromInt(0);
    hit.hitTimeMs      = {};
    world.ecs().get<DisplacementComponent>(h).curveId = -1;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::Death);
    CHECK(world.ecs().get<HitStateComponent>(h).displacementId == -1);
    CHECK(world.ecs().get<DisplacementComponent>(h).curveId == -1);
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
    hit.hitType         = 2;
    motion.completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::Death);
    // 死亡位移来自角色配置（角色 101 为 133）。
    CHECK(world.ecs().get<HitStateComponent>(h).displacementId != -1);
}

// 受击切换死亡不套死亡位移（原版 ActionHitSwitch → dealWithStatus(Death) 无位移参数）。
TEST_CASE("hit family: HitSwitch death applies no death displacement")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    placeHit(world, h, RoleStateId::HitSwitch, true);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    motion.completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::Death);
    CHECK(world.ecs().get<HitStateComponent>(h).displacementId == -1);
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
    hit.hitType        = 2;
    hit.animationEnd   = true;
    hit.displacementEnd = true;
    hit.stiffTime      = Fixed::fromInt(0);
    hit.hitTimeMs      = {};
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::GetUp);
    motion.completedThisFrame = true;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::Wake);
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK(world.ecs().get<RoleStateComponent>(h).state == RoleStateId::Idle);
}

// 地面小怪的死亡位移会把尸体弹起再落地。落地后必须回收，不能停在 Death。
TEST_CASE("hit family: monster death with configured displacement despawns corpse")
{
    World world        = makeWorld();
    const Entity monster = monster21881(world);
    REQUIRE(monster.value != 0);
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(21881);
    REQUIRE(role != nullptr);
    REQUIRE(role->deathDisplacementId == 133);
    const Fixed hp = world.ecs().get<HealthComponent>(monster).hp;
    health::rules::applyDelta(world, monster, -hp, HpCause::Hit, monster, {});
    tickUntilGone(world, monster, 300);
    CHECK_FALSE(world.ecs().alive(monster));
}

// 未给出死亡位移（倒地路径 -1，或套用失败的位移行）时不能停在死亡态；进入 Death 的首帧仍要播死亡动作。
TEST_CASE("hit family: monster death without displacement row still despawns")
{
    World world = makeWorld();
    const Entity floorDeath = monster21881(world);
    REQUIRE(floorDeath.value != 0);
    placeHit(world, floorDeath, RoleStateId::HitFloor, true);
    auto& hit = world.ecs().get<HitStateComponent>(floorDeath);
    hit.hitType         = 2;
    hit.animationEnd    = true;
    hit.displacementEnd = true;
    hit.stiffTime       = Fixed::fromInt(0);
    hit.hitTimeMs       = {};
    world.ecs().get<DisplacementComponent>(floorDeath).curveId = -1;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    REQUIRE(world.ecs().alive(floorDeath));
    CHECK(world.ecs().get<RoleStateComponent>(floorDeath).state == RoleStateId::Death);
    CHECK(world.ecs().get<MotionStateComponent>(floorDeath).motionName == "death");
    CHECK(world.ecs().get<HitStateComponent>(floorDeath).displacementId == -1);
    CHECK_FALSE(world.ecs().has<DespawningComponent>(floorDeath));
    CHECK(world.ecs().get<RoleStateComponent>(floorDeath).stateMs <= Fixed::fromInt(conf::kRoleDeathTime));
    tickUntilGone(world, floorDeath, 300);
    CHECK_FALSE(world.ecs().alive(floorDeath));

    World worldMissing   = makeWorld();
    const Entity missing = monster21881(worldMissing);
    REQUIRE(missing.value != 0);
    placeHit(worldMissing, missing, RoleStateId::Idle, true);
    worldMissing.tick(FrameInput{worldMissing.frame() + 1, {}, {}, {}});
    REQUIRE(worldMissing.ecs().alive(missing));
    REQUIRE(worldMissing.ecs().get<RoleStateComponent>(missing).state == RoleStateId::Death);
    auto& missingHit = worldMissing.ecs().get<HitStateComponent>(missing);
    missingHit.displacementId = 999999;
    if (!displacement::rules::set(worldMissing, missing, 999999))
        missingHit.displacementId = -1;
    CHECK(missingHit.displacementId == -1);
    CHECK(worldMissing.ecs().get<MotionStateComponent>(missing).motionName == "death");
    CHECK_FALSE(worldMissing.ecs().has<DespawningComponent>(missing));
    tickUntilGone(worldMissing, missing, 300);
    CHECK_FALSE(worldMissing.ecs().alive(missing));
}

// 落地制动/倒地标志若没送到死亡体，贴地且不再向上时仍要回收。
TEST_CASE("hit family: monster death despawns after landing when brake and lie were missed")
{
    World world        = makeWorld();
    const Entity monster = monster21881(world);
    REQUIRE(monster.value != 0);
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(21881);
    REQUIRE(role != nullptr);
    REQUIRE(role->deathDisplacementId == 133);
    const Fixed hp = world.ecs().get<HealthComponent>(monster).hp;
    health::rules::applyDelta(world, monster, -hp, HpCause::Hit, monster, {});
    for (int i = 0; i < 10 && world.ecs().alive(monster) &&
         world.ecs().get<RoleStateComponent>(monster).state != RoleStateId::Death; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    REQUIRE(world.ecs().alive(monster));
    REQUIRE(world.ecs().get<RoleStateComponent>(monster).state == RoleStateId::Death);
    CHECK(world.ecs().get<HitStateComponent>(monster).displacementId == 133);
    for (int i = 0; i < 300 && world.ecs().alive(monster); ++i)
    {
        auto& disp = world.ecs().get<DisplacementComponent>(monster);
        disp.curveId = -1;
        disp.brake   = false;
        disp.lie     = false;
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    }
    CHECK_FALSE(world.ecs().alive(monster));
}
