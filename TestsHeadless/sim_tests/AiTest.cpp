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
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.stageId   = 0;
    setup.copyId    = 0;
    setup.aiEnabled = true;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 按配置 id 查找角色实体。
Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == configId && identity.kind == SpawnKind::Role)
            found = entity;
    });
    return found;
}

// 找一个由 AI 驱动的怪物（RoleDef 带 aiId、角色类型为怪物系且非玩家槽位）。
Entity findMonster(World& world)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.kind != SpawnKind::Role)
            return;
        if (identity.playerSlot.has_value())
            return;
        const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
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
    facts.alerted   = false;
    facts.provoked  = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 0);

    facts = {};
    facts.targetInSight = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 1);

    facts = {};
    facts.lastAction = ai::AiAction::Patrol;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 2);

    facts = {};
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 3);

    facts = {};
    facts.alerted    = true;
    facts.hasTarget  = true;
    facts.skillReady = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 4);

    facts = {};
    facts.alerted            = true;
    facts.hasTarget          = true;
    facts.targetOutsideChase = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 5);

    facts = {};
    facts.alerted   = true;
    facts.hasTarget = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 6);

    facts = {};
    facts.alerted = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == 7);
}

// 攻击中不决策。
TEST_CASE("ai pickRule returns no rule while attacking")
{
    World world;
    ai::AiFacts facts;
    facts.inAttack = true;
    facts.alerted  = true;
    facts.hasTarget = true;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == -1);
}

// 移动中只允许更高优先级抢占。
TEST_CASE("ai pickRule allows only higher-priority preemption while moving")
{
    World world;
    ai::AiFacts facts;
    // 追击（下标 5）在跑：同下标及更低优先级都被拦截，返回 -1。
    facts.alerted            = true;
    facts.hasTarget          = true;
    facts.targetOutsideChase = true;
    facts.movingRule         = 5;
    CHECK(ai::rules::pickRule(world, facts, ai::kMonsterRules, std::span<const Fixed>{}) == -1);

    // 巡逻（下标 3）在跑：更高优先级的受击醒技能仍可抢占。
    facts = {};
    facts.provoked   = true;
    facts.movingRule = 3;
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
    CHECK(brain.aiId > 0);
    CHECK(brain.target == monster);  // 哨兵为自身
    // 怪物技能簿非空（醒技能或普通技能）。
    REQUIRE(world.ecs().has<SkillBookComponent>(monster));
    CHECK_FALSE(world.ecs().get<SkillBookComponent>(monster).skills.empty());
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
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).position;
    world.ecs().get<TransformComponent>(hero).position = mpos;
    world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(10);

    ai::rules::updateAgent(world, monster);
    const auto& brain = world.ecs().get<AiBrainComponent>(monster);
    CHECK(brain.target == hero);
    CHECK(brain.alerted);  // 未警戒且索敌发现目标 → 醒技能并进入警戒
}

// 施法意图：已警戒、有目标且技能就绪时写入按键。
TEST_CASE("ai monster writes cast intent when alerted and skill ready")
{
    World world          = makeAiWorld();
    const Entity hero    = roleByConfig(world, 101);
    const Entity monster = findMonster(world);
    REQUIRE(hero.value != 0);
    REQUIRE(monster.value != 0);
    ai::rules::ensureBrain(world, monster, 1);

    auto& brain = world.ecs().get<AiBrainComponent>(monster);
    brain.alerted = true;
    brain.target  = hero;
    for (auto& interval : brain.skillIntervalMs)
        interval = {};
    if (brain.skillIntervalMs.empty())
        brain.skillIntervalMs.push_back({});
    DrawPolicy policy;
    world.setDrawPolicy(&policy);
    auto& book = world.ecs().get<SkillBookComponent>(monster);
    REQUIRE_FALSE(book.skills.empty());
    for (auto& skill : book.skills)
    {
        skill.aiLoadCd  = {};
        skill.aiCheckCd = {};
    }
    int32_t distX = 80;
    if (book.skills[0].aiConfigId > 0)
        if (const auto* gate = conf::Config::instance().find<conf::battle::SkillAiDef>(book.skills[0].aiConfigId);
            gate && gate->oppDisX.size() >= 2)
            distX = std::max(1, (gate->oppDisX[0] + gate->oppDisX[1]) / 2);
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).position;
    world.ecs().get<TransformComponent>(hero).position = mpos;
    world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(distX);
    auto& intent = world.ecs().get<IntentComponent>(monster);
    intent.buttons.clear();

    ai::rules::updateAgent(world, monster);
    CHECK(intent.presetSkillIndex >= 0);
    CHECK(intent.autoCast);
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
    brain.alerted = true;
    brain.target  = hero;
    brain.skillIntervalMs = {Fixed::fromInt(60000)};  // 技能未就绪，排除 CastBestSkill
    // 玩家放到远处，落在追击范围外。
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).position;
    world.ecs().get<TransformComponent>(hero).position = mpos;
    world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(100000);

    auto& intent = world.ecs().get<IntentComponent>(monster);
    intent.move.mode = MoveMode::None;
    ai::rules::updateAgent(world, monster);
    CHECK(intent.move.mode == MoveMode::Walk);
    CHECK(brain.movingRule >= 0);
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
            if (brain.alerted)
                moved = true;
            if (intent.presetSkillIndex >= 0 || !intent.buttons.empty())
                cast = true;
        });
    }
    CHECK(moved);
    CHECK(cast);
}

// 出生装配：房间生成怪物时把 SpawnRequest.aiDifficulty 落盘到 IdentityComponent。
TEST_CASE("ai role identity persists spawn aiDifficulty")
{
    World world = makeAiWorld();
    const Entity monster = findMonster(world);
    REQUIRE(monster.value != 0);
    const int32_t expected = world.ecs().singleton<mugen::sim::RoomState>().config.monsterAiDifficulty;
    CHECK(world.ecs().get<IdentityComponent>(monster).aiDifficulty == expected);
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
    const auto* role     = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
    REQUIRE(role != nullptr);
    REQUIRE_FALSE(role->aiId.empty());
    REQUIRE(world.ecs().has<AiBrainComponent>(monster));
    const size_t difficulty = identity.aiDifficulty > 0 &&
                                      static_cast<size_t>(identity.aiDifficulty) <= role->aiId.size()
                                  ? static_cast<size_t>(identity.aiDifficulty - 1)
                                  : role->aiId.size() - 1;
    CHECK(world.ecs().get<AiBrainComponent>(monster).aiId == role->aiId[difficulty]);
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
    brain.alerted = true;
    brain.target  = hero;
    brain.skillIntervalMs = {Fixed::fromInt(60000)};  // 技能未就绪，排除 CastBestSkill
    // 玩家放到远处，落在追击范围外。
    const FixedVec3 mpos = world.ecs().get<TransformComponent>(monster).position;
    world.ecs().get<TransformComponent>(hero).position = mpos;
    world.ecs().get<TransformComponent>(hero).position.x += Fixed::fromInt(100000);

    auto& intent = world.ecs().get<IntentComponent>(monster);
    intent.move.mode = MoveMode::None;
    ai::rules::updateAgent(world, monster);
    // 追击写入行走态（与旧实现 AiMove 的 Patrol/Chase/Alert → "running" 一致）。
    CHECK(intent.move.mode == MoveMode::Walk);
    // 状态 → 动作映射：Walk → "running"，Run → "walk"（怪物 motion 只含前者）。
    CHECK(std::strcmp(role::motionName(RoleStateId::Walk), "running") == 0);
    CHECK(std::strcmp(role::motionName(RoleStateId::Run), "walk") == 0);
}
