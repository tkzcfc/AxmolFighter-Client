#include "doctest.h"
#include "BuffTestSupport.h"
#include "mugen/sim/World.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/conf/battle/HitDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

// hitType == -1 命中：计入命中次数并去重，但不扣血、不进受击状态、不发 HitEvent / HitStopEvent（原版语义）。
// 覆盖的配置：特效 920430 → hitId 920280（hitType == -1）。
namespace
{
// 构造含主控英雄的战斗世界（职业 1 / 角色 101，等级 1000，关 AI）。
World makeWorld()
{
    buff_test::loadAssets();
    auto& avatar = *AvatarAssetCache::getInstance();
    if (!avatar.isLoaded())
        REQUIRE(avatar.load("/mugen/config/avatar.bin"));
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

// 取主控玩家实体。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().m_entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}
}  // namespace

TEST_CASE("combat hitType -1 records hit count but deals no damage or hit reaction")
{
    World world       = makeWorld();
    const Entity target = hero(world);
    REQUIRE(target.value != 0);

    // 攻击者：camp 8，敌视阵营 2（英雄阵营）。
    const Entity attacker = buff_test::spawnActor(world, 1, 8, 2);

    // 特效 920430 → hitId 920280（hitType == -1）。
    constexpr int32_t kEffectId = 920430;
    const auto* effectRow       = conf::Config::instance().find<conf::battle::EffectDef>(kEffectId);
    REQUIRE(effectRow != nullptr);
    REQUIRE(effectRow->hitId == 920280);
    const auto* hitRow = conf::Config::instance().find<conf::battle::HitDef>(effectRow->hitId);
    REQUIRE(hitRow != nullptr);
    REQUIRE(hitRow->hitType == -1);

    const Entity effect = world.ecs().create();
    IdentityComponent effectIdn;
    effectIdn.m_kind     = SpawnKind::Effect;
    effectIdn.m_configId = kEffectId;
    effectIdn.m_camp     = 8;
    world.ecs().add(effect, effectIdn);
    EffectInfoComponent info;
    info.m_configId = kEffectId;
    info.m_owner    = attacker;
    world.ecs().add(effect, info);
    world.ecs().add(effect, HitTrackerComponent{});

    // 固定属性闪避策略：dodge 抽样 100 保证永不属性闪避，命中走完微管线。
    DrawPolicy policy;
    policy.m_dodge = Fixed::fromInt(100);
    world.setDrawPolicy(&policy);

    const Fixed hpBefore = world.ecs().get<HealthComponent>(target).m_hp;

    ContactEvent contact;
    contact.m_a     = effect;
    contact.m_b     = target;
    contact.m_phase = ContactPhase::Began;
    world.ecs().singleton<EventQueue<ContactEvent>>().push(contact);
    runCombat(world);

    // 命中次数已记，且按目标去重（hitInterval == -1）。
    const auto& tracker = world.ecs().get<HitTrackerComponent>(effect);
    CHECK(tracker.m_hitCount == 1);
    CHECK(tracker.m_hitTargets.size() == 1);
    // 无伤害。
    CHECK(world.ecs().get<HealthComponent>(target).m_hp == hpBefore);
    // 不排队受击请求（不进 Hit）。
    CHECK(world.ecs().get<HitStateComponent>(target).m_pendingHitDefId == 0);
    // 不发命中 / 顿帧事实（摘要无 hit/dmg/stop 行）。
    CHECK(world.ecs().singleton<EventQueue<HitEvent>>().m_items.empty());
    CHECK(world.ecs().singleton<EventQueue<HitStopEvent>>().m_items.empty());
}
