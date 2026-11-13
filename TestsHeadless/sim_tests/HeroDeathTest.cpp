#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

// 结构体 RoomState 与枚举 mugen::RoomState 同名，用别名区分。
using Room = mugen::sim::RoomState;

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

// 构造指定房间的战斗世界（职业 1 / 角色 101，等级 1000，关 AI）。
World makeWorld(int32_t roomId)
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = roomId;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(roomId);
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

// 取得主控玩家实体（首个玩家槽位）。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().m_entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}

// 推进 frames 帧空输入。
void tickN(World& world, uint64_t frames)
{
    for (uint64_t i = 0; i < frames; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
}
}  // namespace

// 英雄死亡后不会被回收：推进到死亡体结束（旧实现会在此时销毁英雄），全程不崩，
// 房间保持在等待复活状态。
TEST_CASE("hero death: survives death body, room waits in FightRevive")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    room.m_maxRevives = 1;
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    const Fixed hp = world.ecs().get<HealthComponent>(h).m_hp;
    health::rules::applyDelta(world, h, -hp, HpCause::Hit, h, {});
    // 推进到死亡位移/死亡动画结束之后（远超过死亡体时长），英雄仍应存活并停留在 Death。
    tickN(world, 300);
    CHECK(world.ecs().alive(h));
    CHECK_FALSE(world.ecs().has<DespawningComponent>(h));
    CHECK(room.m_reviving);
    CHECK_FALSE(room.m_failure);
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Death);
}

// 有复活次数时，英雄死亡经 ReviveCommand 复活回到 Idle，全程不崩。
TEST_CASE("hero death: revive command returns hero to Idle after death body")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    room.m_maxRevives = 1;
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    const Fixed hp = world.ecs().get<HealthComponent>(h).m_hp;
    health::rules::applyDelta(world, h, -hp, HpCause::Hit, h, {});
    tickN(world, 300);
    REQUIRE(world.ecs().alive(h));
    REQUIRE(room.m_reviving);

    FrameInput input;
    input.frame = world.frame() + 1;
    input.systemCommands.push_back(ReviveCommand{0});
    world.tick(input);
    CHECK_FALSE(room.m_reviving);
    CHECK(room.m_usedRevives == 1);
    CHECK(world.ecs().get<RoleStateComponent>(h).m_reviveRequested);
    tickN(world, 1);
    const auto& health = world.ecs().get<HealthComponent>(h);
    CHECK(health.m_hp == health.m_maxHp);
    CHECK_FALSE(health.m_dead);
    CHECK(world.ecs().get<RoleStateComponent>(h).m_state == RoleStateId::Idle);
}

// 无复活次数时，英雄死亡进入 Failure，英雄实体保留（不被回收）。
TEST_CASE("hero death: no revives enters failure, hero stays alive")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<Room>();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    const Fixed hp = world.ecs().get<HealthComponent>(h).m_hp;
    health::rules::applyDelta(world, h, -hp, HpCause::Hit, h, {});
    tickN(world, 300);
    CHECK(room.m_failure);
    CHECK_FALSE(room.m_victory);
    CHECK(world.ecs().alive(h));
    CHECK_FALSE(world.ecs().has<DespawningComponent>(h));
}
