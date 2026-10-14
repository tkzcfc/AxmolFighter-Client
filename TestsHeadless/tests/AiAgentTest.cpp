#include "SkillTestSupport.h"
#include "mugen/sim/ai/AiAgent.h"
#include "mugen/sim/buff/BuffRules.h"

using namespace skill_test;

namespace
{
struct MonsterFixture
{
    World world;
    conf::ConfigArchive archive;
    conf::AiConfig aiRow;
    int32_t roleId = 0;
    int32_t skillId = 0;
    Entity monster;
    MonsterFixture()
    {
        loadConfig();
#define COPY_AI_TABLE(kind, Type, Key) archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(COPY_AI_TABLE)
#undef COPY_AI_TABLE
        for (auto& role : archive.role)
            if (role.roleType == 2 && !role.aiId.empty())
            {
                roleId = role.id;
                role.velocity = "1"_fx;
                role.radius = 1;
                aiRow = conf::Config::instance().get<conf::AiConfig>(role.aiId.front());
                break;
            }
        REQUIRE(roleId != 0);
        skillId = findSkill([](const auto& row) { return row.id == 921030; });
        for (auto& row : archive.skill)
            if (row.id == skillId)
            {
                row.nextSkill = -1;
                row.cd = 10000;
                row.mp = 0;
                row.ep = 0;
            }
        aiRow.targetScopeX = {-10, 10}; aiRow.targetScopeZ = {-10, 10};
        aiRow.patrolScopeX = {100, 100}; aiRow.patrolScopeZ = {50, 50};
        aiRow.patrolDelayTime = {100, 150};
        aiRow.chaseScopeX = {20, 40}; aiRow.chaseScopeZ = {10, 20};
        aiRow.chaseDelayTime = {100, 150}; aiRow.alertDelayTime = {100, 150};
        aiRow.skillPriorityLevel = {{1}, {3}, {3}};
        aiRow.skillPriorityLevelCd = {{100}, {200}, {300}};
        aiRow.skillInterval = 500;
        publish();
        world.init({});
        world.ecs().singleton<MapRegion>().moveRanges = {{"-2000"_fx, "-2000"_fx, "4000"_fx, "4000"_fx}};
        monster = create(true, 8, {});
    }
    ~MonsterFixture() { conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"); }
    void publish()
    {
        for (auto& row : archive.ai) if (row.id == aiRow.id) row = aiRow;
        ByteBuffer bytes;
        for (auto byte : conf::kConfigMagic) bytes.writeByte(byte);
        bytes.writeUint32(conf::kConfigVersion); bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter(bytes).write(archive); bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }
    Entity create(bool aiEnabled, int camp, FixedVec3 pos)
    {
        const auto entity = world.create(GroupKind::Role);
        auto& ecs = world.ecs();
        RoleIdentity identity; identity.roleId = roleId; identity.roleType = 2;
        identity.aiId = aiRow.id; identity.isAi = aiEnabled; identity.camp = camp;
        ecs.add(entity, identity); ecs.add(entity, RoleStatus{}); ecs.add(entity, sim::ExtraState{});
        Vitals vitals; vitals.hp = vitals.maxHp = "1000"_fx;
        vitals.mp = vitals.maxMp = "1000000"_fx; vitals.ep = vitals.maxEp = "1000000"_fx;
        Transform transform;
        transform.position = pos;
        ecs.add(entity, vitals); ecs.add(entity, transform); ecs.add(entity, RoleHitState{});
        ecs.add(entity, RoleTimers{}); ecs.add(entity, Attributes{}); ecs.add(entity, ColliderState{{}, {}, 0, "1"_fx});
        ecs.add(entity, Displacement{}); ecs.add(entity, MotionState{}); ecs.add(entity, ControlEventQueue{});
        ecs.add(entity, SkillPool{}); ecs.add(entity, RoleBrain{}); ecs.add(entity, RoleControl{});
        if (aiEnabled) ecs.add(entity, AiAgent{});
        ecs.get<RoleBrain>(entity).m_attackLayout = behavior::definitionKey(world, entity);
        return entity;
    }
    void tick()
    {
        FrameInput input; input.frame = world.ecs().singleton<WorldClock>().frame + 1;
        world.tick(input);
    }
    void load(bool special = false)
    {
        skill::SkillLoadout data(5);
        if (special) data[4] = {{{{skillId, -1}}}, {{{-1, -1}}}, {{{skillId, -1}}}};
        else for (int i = 0; i < 3; ++i) data[0].push_back({{{skillId, -1}}});
        skill::loadSkill(world, monster, data);
        world.ecs().get<SkillPool>(monster).m_crystals = 100000;
    }
    ai::ActionMove* movement()
    {
        if (auto* move = std::get_if<ai::ActionMove>(&world.ecs().get<RoleBrain>(monster).m_data)) return move;
        return nullptr;
    }
};
}

TEST_CASE("Ai monster patrol movement pauses for randomized dwell and checks target every frame")
{
    MonsterFixture f;
    f.tick();
    REQUIRE(status::isPatrolStatus(f.world.ecs().get<RoleStatus>(f.monster)));
    REQUIRE(f.movement());
    CHECK(f.movement()->m_delayMs >= "100"_fx);
    CHECK(f.movement()->m_delayMs < "150"_fx);
    for (int i = 0; i < 80 && !f.movement()->m_complete; ++i) f.tick();
    REQUIRE(f.movement()->m_complete);
    const auto goal = f.world.ecs().get<RoleStatus>(f.monster).entityParams.move_pos;
    CHECK(f.world.ecs().get<Transform>(f.monster).position.x == goal.x);
    CHECK(f.world.ecs().get<Transform>(f.monster).position.z == goal.z);
    SUBCASE("dwell excludes travel and ends strictly after delay")
    {
        CHECK(f.movement()->m_timeMs == Fixed{});
        f.tick();
        CHECK(status::isPatrolStatus(f.world.ecs().get<RoleStatus>(f.monster)));
        CHECK(f.world.ecs().get<Transform>(f.monster).position.x == goal.x);
        for (int i = 0; i < 20 && status::isPatrolStatus(f.world.ecs().get<RoleStatus>(f.monster)); ++i) f.tick();
        CHECK_FALSE(status::isPatrolStatus(f.world.ecs().get<RoleStatus>(f.monster)));
        CHECK(f.world.ecs().get<AiAgent>(f.monster).m_tree.m_alert == ai::AlertStatus::NoAlert);
    }
    SUBCASE("patrol discovers a target during dwell")
    {
        const auto target = f.create(false, 2, goal);
        f.tick();
        CHECK(f.world.ecs().get<SkillPool>(f.monster).m_target == target);
        CHECK_FALSE(status::isPatrolStatus(f.world.ecs().get<RoleStatus>(f.monster)));
    }
}

TEST_CASE("Ai monster hit recovery casts X before Alert and nonalert attack uses Z")
{
    MonsterFixture f;
    const auto target = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    f.load(true);
    auto& ecs = f.world.ecs();
    auto& pool = ecs.get<SkillPool>(f.monster);
    pool.m_target = target;
    SUBCASE("hit X path")
    {
        ecs.get<RoleStatus>(f.monster).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::Hit;
        ai::update(f.world, f.monster);
        CHECK(pool.m_current == kNoSkill);
        ecs.get<RoleStatus>(f.monster).eEntityStatus = EntityRoleStatus::Idle;
        ai::update(f.world, f.monster);
        REQUIRE(pool.m_current != kNoSkill);
        CHECK(skill::baseSlot(pool, pool.m_skills[pool.m_current]) == "X");
        CHECK(status::isAttackStatus(ecs.get<RoleStatus>(f.monster)));
        CHECK(ecs.get<AiAgent>(f.monster).m_tree.m_alert == ai::AlertStatus::NoAlert);
        skill::castBegan(f.world, f.monster, pool.m_current);
        ai::update(f.world, f.monster);
        CHECK(ecs.get<AiAgent>(f.monster).m_tree.m_alert == ai::AlertStatus::NoAlert);
        idle(f.world, f.monster);
        ai::update(f.world, f.monster);
        CHECK(ecs.get<AiAgent>(f.monster).m_tree.m_alert == ai::AlertStatus::Alert);
    }
    SUBCASE("Z ignores auto gating")
    {
        CHECK(ai::invoke(f.world, f.monster, ai::Method::ExecuteRandAttack) == ai::Result::Running);
        REQUIRE(pool.m_current != kNoSkill);
        CHECK(skill::baseSlot(pool, pool.m_skills[pool.m_current]) == "Z");
    }
}

TEST_CASE("Ai monster Alert chases to target near-side bounds and wanders within range")
{
    MonsterFixture f;
    auto& ecs = f.world.ecs();
    const auto target = f.create(false, 2, {"200"_fx, {}, "100"_fx});
    ecs.get<SkillPool>(f.monster).m_target = target;
    ecs.get<AiAgent>(f.monster).m_tree.m_alert = ai::AlertStatus::Alert;
    f.tick();
    CHECK(status::isChaseStatus(ecs.get<RoleStatus>(f.monster)));
    CHECK(ecs.get<RoleStatus>(f.monster).entityParams.move_pos.x == "160"_fx);
    CHECK(ecs.get<RoleStatus>(f.monster).entityParams.move_pos.z == "80"_fx);
    REQUIRE(f.movement());
    for (int i = 0; i < 120 && !f.movement()->m_complete; ++i) f.tick();
    REQUIRE(f.movement()->m_complete);
    CHECK(ecs.get<Transform>(f.monster).position.x == "160"_fx);
    CHECK(ecs.get<Transform>(f.monster).position.z == "80"_fx);
    for (int i = 0; i < 30 && !status::isAlertStatus(ecs.get<RoleStatus>(f.monster)); ++i) f.tick();
    REQUIRE(status::isAlertStatus(ecs.get<RoleStatus>(f.monster)));
    const auto goal = ecs.get<RoleStatus>(f.monster).entityParams.move_pos;
    CHECK(goal.x >= "160"_fx); CHECK(goal.x <= "180"_fx);
    CHECK(goal.z >= "80"_fx); CHECK(goal.z <= "90"_fx);
}

TEST_CASE("Ai monster selects priority then index resets peers and consumes SkillAi gating")
{
    MonsterFixture f;
    const auto target = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    f.load();
    auto& pool = f.world.ecs().get<SkillPool>(f.monster);
    pool.m_target = target;
    CHECK(ai::invoke(f.world, f.monster, ai::Method::ExecuteAttack) == ai::Result::Success);
    REQUIRE(pool.m_aiSkillCastInterval.size() == 3);
    CHECK(pool.m_aiSkillCastInterval[1] == "200"_fx);
    pool.m_aiSkillCastInterval = {Fixed{}, Fixed{}, Fixed{}};
    SUBCASE("equal priority uses original index")
    {
        REQUIRE(ai::invoke(f.world, f.monster, ai::Method::ExecuteAttack) == ai::Result::Running);
        CHECK(skill::baseSlot(pool, pool.m_skills[pool.m_current]) == "B");
    }
    SUBCASE("SkillAi blocks first ready high priority slot")
    {
        const auto b = skill::getGivenSkill(pool, "B");
        REQUIRE(b != kNoSkill);
        pool.m_skills[b].m_ai.m_configId = f.archive.skill_ai.front().id;
        pool.m_skills[b].m_ai.m_loadCd = "1000"_fx;
        REQUIRE(ai::invoke(f.world, f.monster, ai::Method::ExecuteAttack) == ai::Result::Running);
        CHECK(skill::baseSlot(pool, pool.m_skills[pool.m_current]) == "C");
    }
    CHECK(pool.m_aiSkillCastInterval[0] == Fixed{});
    CHECK(pool.m_aiSkillCastInterval[1] == "200"_fx);
    CHECK(pool.m_aiSkillCastInterval[2] == "300"_fx);
    skill::update(f.world, f.monster);
    CHECK(pool.m_aiSkillCastInterval[1] < "200"_fx);
}

TEST_CASE("Ai monster retargets dead enemies and preserves Alert with no targets")
{
    MonsterFixture f;
    auto& ecs = f.world.ecs();
    const auto first = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    const auto second = f.create(false, 4, {"200"_fx, {}, "50"_fx});
    ecs.get<SkillPool>(f.monster).m_target = first;
    ecs.get<Vitals>(first).hp = {};
    REQUIRE(ai::invoke(f.world, f.monster, ai::Method::CheckAttack) == ai::Result::Success);
    CHECK(ecs.get<SkillPool>(f.monster).m_target == second);
    ecs.get<Vitals>(second).hp = {};
    ecs.get<AiAgent>(f.monster).m_tree.m_alert = ai::AlertStatus::Alert;
    ai::update(f.world, f.monster);
    CHECK(ecs.get<SkillPool>(f.monster).m_target == f.monster);
    CHECK(ecs.get<AiAgent>(f.monster).m_tree.m_alert == ai::AlertStatus::Alert);
    CHECK_FALSE(status::isPatrolStatus(ecs.get<RoleStatus>(f.monster)));
    CHECK(ai::candidates(f.world, f.monster).empty());
}

TEST_CASE("Ai monster threat changes every ten seconds and taunt forces target before sight")
{
    MonsterFixture f;
    auto& ecs = f.world.ecs();
    const auto first = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    const auto second = f.create(false, 4, {"200"_fx, {}, "50"_fx});
    ecs.get<SkillPool>(f.monster).m_target = first;
    ecs.get<AiAgent>(f.monster).m_tree.m_alert = ai::AlertStatus::Alert;
    ai::addThreat(f.world, f.monster, first, "10"_fx);
    ai::addThreat(f.world, f.monster, second, "20"_fx);
    ecs.get<AiAgent>(f.monster).m_changeTargetMs = "10000"_fx - ecs.singleton<WorldClock>().dtMs * "2"_fx;
    ai::update(f.world, f.monster);
    CHECK(ecs.get<SkillPool>(f.monster).m_target == first);
    ai::update(f.world, f.monster);
    CHECK(ecs.get<SkillPool>(f.monster).m_target == second);
    CHECK(ecs.get<AiAgent>(f.monster).m_threat.empty());
    ecs.get<RoleTimers>(f.monster).m_sneerRecounts = {{first, "100"_fx}, {second, "1"_fx}};
    REQUIRE(buff::sneerTarget(f.world, f.monster) == first);
    CHECK(ai::checkTarget(f.world, f.monster) == ai::Result::Success);
    CHECK(ecs.get<SkillPool>(f.monster).m_target == first);
    const auto elapsed = ecs.get<AiAgent>(f.monster).m_changeTargetMs;
    ai::update(f.world, f.monster);
    CHECK(ecs.get<SkillPool>(f.monster).m_target == first);
    CHECK(ecs.get<AiAgent>(f.monster).m_changeTargetMs == elapsed);
}

TEST_CASE("Ai monster movement and random stream survive clone and snapshot")
{
    MonsterFixture f;
    f.tick();
    auto clone = f.world.clone();
    ByteBuffer bytes; f.world.serialize(bytes);
    World restored; REQUIRE(restored.deserialize(bytes));
    for (int i = 0; i < 35; ++i)
    {
        FrameInput input; input.frame = f.world.ecs().singleton<WorldClock>().frame + 1;
        f.world.tick(input); clone.tick(input); restored.tick(input);
        CHECK(f.world.hash() == clone.hash());
        CHECK(f.world.hash() == restored.hash());
    }
}
