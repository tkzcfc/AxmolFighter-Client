#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/ai/AiRules.h"
#include "mugen/sim/ai/AiComponents.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/RoleDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <cstring>
#include <span>

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

// 构造一个开启 AI 的战斗世界：职业 1 / 角色 101 进入副本 101（房间 2010101）。
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
    player.m_roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.m_level  = 1000;
    player.m_camp   = 2;
    player.m_slot   = 0;
    player.m_isAi   = false;
    setup.m_players.push_back(player);
    setup.m_stageId   = 0;
    setup.m_copyId    = 0;
    setup.m_aiEnabled = true;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 按配置 id 查找角色实体。
Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.m_configId == configId && identity.m_kind == SpawnKind::Role)
            found = entity;
    });
    return found;
}

// 找一个由 AI 驱动的怪物（RoleDef 带 aiId、角色类型为怪物系且非玩家槽位）。
Entity findMonster(World& world)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.m_kind != SpawnKind::Role)
            return;
        if (identity.m_playerSlot.has_value())
            return;
        const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
        if (role && !role->aiId.empty() && role->roleType >= 2)
            found = entity;
    });
    return found;
}
}  // namespace

// 规则命中：按优先级返回第一条命中规则。
TEST_CASE("ai pickRule returns first matching rule by priority")
{
    World world;  // 规则表不含 ChancePercent，无需初始化世界。

    ai::AiFacts facts;
    facts.m_alerted   = false;
    facts.m_provoked  = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 0);

    facts = {};
    facts.m_targetInSight = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 1);

    facts = {};
    facts.m_lastAction = ai::AiAction::Patrol;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 2);

    facts = {};
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 3);

    facts = {};
    facts.m_alerted    = true;
    facts.m_hasTarget  = true;
    facts.m_skillReady = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 4);

    facts = {};
    facts.m_alerted            = true;
    facts.m_hasTarget          = true;
    facts.m_targetOutsideChase = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 5);

    facts = {};
    facts.m_alerted   = true;
    facts.m_hasTarget = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 6);

    facts = {};
    facts.m_alerted = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 7);
}

// 攻击中不决策。
TEST_CASE("ai pickRule returns no rule while attacking")
{
    World world;
    ai::AiFacts facts;
    facts.m_inAttack = true;
    facts.m_alerted  = true;
    facts.m_hasTarget = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == -1);
}

// 移动中只允许更高优先级抢占。
TEST_CASE("ai pickRule allows only higher-priority preemption while moving")
{
    World world;
    ai::AiFacts facts;
    // 追击（下标 5）在跑：同下标及更低优先级都被拦截，返回 -1。
    facts.m_alerted            = true;
    facts.m_hasTarget          = true;
    facts.m_targetOutsideChase = true;
    facts.m_movingRule         = 5;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == -1);

    // 巡逻（下标 3）在跑：更高优先级的受击醒技能仍可抢占。
    facts = {};
    facts.m_provoked   = true;
    facts.m_movingRule = 3;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 0);
}

// ensureBrain：为怪物挂上 AiBrain 组件与怪物技能簿。
TEST_CASE("ai ensureBrain attaches brain and monster skill book")
{
    World world = makeAiWorld();
    const Entity monster = findMonster(world);
    REQUIRE(monster.value != 0);
    CHECK_FALSE(world.ecs().has<AiBrainComponent>(monster));

    ai::rules::ensureBrain(world, monster, 1);
    REQUIRE(world.ecs().has<AiBrainComponent>(monster));
    const auto& brain = world.ecs().get<AiBrainComponent>(monster);
    CHECK(brain.m_aiId > 0);
    CHECK(brain.m_target == monster);  // 哨兵为自身
    // 怪物技能簿非空（醒技能或普通技能）。
    REQUIRE(world.ecs().has<SkillBookComponent>(monster));
    CHECK_FALSE(world.ecs().get<SkillBookComponent>(monster).m_skills.empty());
}

// 选目标：把玩家放到索敌范围内后，怪物锁定玩家。
TEST_CASE("ai monster locks the player within sight range")
{
    World world      = makeAiWorld();
    const Entity hero = roleByConfig(world, 101);
    const Entity monster = findMonster(world);
    REQUIRE(hero.value != 0);
    REQUIRE(monster.value != 0);
    ai::rules::ensureBrain(world, monster, 1);

    // 玩家紧贴怪物，落在索敌区间内。
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).m_position;
    world.ecs().get<TransformComponent>(hero).m_position = mpos;
    world.ecs().get<TransformComponent>(hero).m_position.x += Fixed::fromInt(10);

    ai::rules::updateAgent(world, monster);
    const auto& brain = world.ecs().get<AiBrainComponent>(monster);
    CHECK(brain.m_target == hero);
    CHECK(brain.m_alerted);  // 未警戒且索敌发现目标 → 醒技能并进入警戒
}

// 施法意图：已警戒、有目标且技能就绪时写入按键。
TEST_CASE("ai monster writes cast intent when alerted and skill ready")
{
    World world       = makeAiWorld();
    const Entity monster = findMonster(world);
    REQUIRE(monster.value != 0);
    ai::rules::ensureBrain(world, monster, 1);

    auto& brain = world.ecs().get<AiBrainComponent>(monster);
    brain.m_alerted   = true;
    brain.m_target    = roleByConfig(world, 101);
    // 清空施放间隔，保证 SkillReady 成立。
    brain.m_skillIntervalMs = {Fixed{}};
    auto& intent = world.ecs().get<IntentComponent>(monster);
    intent.m_buttons.clear();

    ai::rules::updateAgent(world, monster);
    CHECK_FALSE(intent.m_buttons.empty());  // 写入了施法按键
    CHECK(intent.m_autoCast);
}

// 追击写 Intent：已警戒、有目标且目标在追击范围外时写移动意图（行走态）。
TEST_CASE("ai monster writes move intent when chasing a target")
{
    World world       = makeAiWorld();
    const Entity hero = roleByConfig(world, 101);
    const Entity monster = findMonster(world);
    REQUIRE(hero.value != 0);
    REQUIRE(monster.value != 0);
    ai::rules::ensureBrain(world, monster, 1);

    auto& brain = world.ecs().get<AiBrainComponent>(monster);
    brain.m_alerted = true;
    brain.m_target  = hero;
    brain.m_skillIntervalMs = {Fixed::fromInt(60000)};  // 技能未就绪，排除 CastBestSkill
    // 玩家放到远处，落在追击范围外。
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).m_position;
    world.ecs().get<TransformComponent>(hero).m_position = mpos;
    world.ecs().get<TransformComponent>(hero).m_position.x += Fixed::fromInt(100000);

    auto& intent = world.ecs().get<IntentComponent>(monster);
    intent.m_move.m_mode = MoveMode::None;
    ai::rules::updateAgent(world, monster);
    CHECK(intent.m_move.m_mode == MoveMode::Walk);
    CHECK(brain.m_movingRule >= 0);
}

// 开 AI 冒烟：副本 101 跑若干秒，怪物能决策（追击/施法）且无断言。
TEST_CASE("ai smoke: room 2010101 monsters act without assertion")
{
    World world = makeAiWorld();
    bool cast   = false;
    bool moved  = false;
    for (uint64_t frame = 1; frame <= 600; ++frame)
    {
        world.tick(FrameInput{frame, {}, {}, {}});
        world.ecs().each<AiBrainComponent, IntentComponent>([&](Entity, const AiBrainComponent& brain,
                                                                const IntentComponent& intent) {
            if (brain.m_alerted)
                moved = true;
            if (!intent.m_buttons.empty())
                cast = true;
        });
    }
    CHECK(moved);
    CHECK(cast);
}

// 出生装配：房间生成怪物时把 SpawnRequest.m_aiDifficulty 落盘到 IdentityComponent。
TEST_CASE("ai role identity persists spawn aiDifficulty")
{
    World world = makeAiWorld();
    const Entity monster = findMonster(world);
    REQUIRE(monster.value != 0);
    const int32_t expected = world.ecs().singleton<mugen::sim::RoomState>().m_config.monsterAiDifficulty;
    CHECK(world.ecs().get<IdentityComponent>(monster).m_aiDifficulty == expected);
}

// 装配大脑：AiSystem 按实体的难度解析 aiId，而非写死默认难度。
TEST_CASE("ai system resolves aiId from persisted difficulty")
{
    World world = makeAiWorld();
    // 一帧让 AiSystem 按实体难度装配大脑。
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    const Entity monster = findMonster(world);
    REQUIRE(monster.value != 0);
    const auto& identity = world.ecs().get<IdentityComponent>(monster);
    const auto* role     = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
    REQUIRE(role != nullptr);
    REQUIRE_FALSE(role->aiId.empty());
    REQUIRE(world.ecs().has<AiBrainComponent>(monster));
    const size_t difficulty = identity.m_aiDifficulty > 0 &&
                                      static_cast<size_t>(identity.m_aiDifficulty) <= role->aiId.size()
                                  ? static_cast<size_t>(identity.m_aiDifficulty - 1)
                                  : role->aiId.size() - 1;
    CHECK(world.ecs().get<AiBrainComponent>(monster).m_aiId == role->aiId[difficulty]);
}

// 玩家槽位角色不被怪物 AI 装配大脑（与旧实现 isAi=false 语义一致）。
TEST_CASE("ai system does not attach brain to player-controlled hero")
{
    World world       = makeAiWorld();
    const Entity hero = roleByConfig(world, 101);
    REQUIRE(hero.value != 0);
    // 玩家角色持有控制组件（有玩家槽位）。
    REQUIRE(world.ecs().has<PlayerControlComponent>(hero));

    // 一帧让 AiSystem 装配怪物大脑。
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK_FALSE(world.ecs().has<AiBrainComponent>(hero));

    // 怪物（无玩家槽位）正常装配大脑。
    const Entity monster = findMonster(world);
    REQUIRE(monster.value != 0);
    CHECK(world.ecs().has<AiBrainComponent>(monster));
}

// 怪物移动意图解析到 "running" 动作：Walk 状态 → "running"（怪物 motion 含），
// Run 状态 → "walk"（玩家普通移动用，怪物 motion 通常不含）。
TEST_CASE("ai monster move intent resolves to running motion")
{
    World world           = makeAiWorld();
    const Entity hero     = roleByConfig(world, 101);
    const Entity monster  = findMonster(world);
    REQUIRE(hero.value != 0);
    REQUIRE(monster.value != 0);
    ai::rules::ensureBrain(world, monster, 1);

    auto& brain = world.ecs().get<AiBrainComponent>(monster);
    brain.m_alerted = true;
    brain.m_target  = hero;
    brain.m_skillIntervalMs = {Fixed::fromInt(60000)};  // 技能未就绪，排除 CastBestSkill
    // 玩家放到远处，落在追击范围外。
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).m_position;
    world.ecs().get<TransformComponent>(hero).m_position = mpos;
    world.ecs().get<TransformComponent>(hero).m_position.x += Fixed::fromInt(100000);

    auto& intent = world.ecs().get<IntentComponent>(monster);
    intent.m_move.m_mode = MoveMode::None;
    ai::rules::updateAgent(world, monster);
    // 追击写入行走态（与旧实现 AiMove 的 Patrol/Chase/Alert → "running" 一致）。
    CHECK(intent.m_move.m_mode == MoveMode::Walk);
    // 状态 → 动作映射：Walk → "running"，Run → "walk"（怪物 motion 只含前者）。
    CHECK(std::strcmp(role::motionName(RoleStateId::Walk), "running") == 0);
    CHECK(std::strcmp(role::motionName(RoleStateId::Run), "walk") == 0);
}
