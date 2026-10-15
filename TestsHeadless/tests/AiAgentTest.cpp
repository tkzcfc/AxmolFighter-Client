#include "SkillTestSupport.h"
#include "mugen/sim/ai/AiAgent.h"
#include "mugen/sim/buff/BuffHolder.h"

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
        skill::SkillCaster(world, monster).loadSkill(data);
        world.ecs().get<SkillPool>(monster).m_crystals = 100000;
    }
    ai::ActionMove* movement()
    {
        if (auto* move = std::get_if<ai::ActionMove>(&world.ecs().get<RoleBrain>(monster).m_data)) return move;
        return nullptr;
    }
};
}

TEST_CASE("Ai monster patrol movement pauses for randomized dwell and discovers targets")
{
    MonsterFixture f;
    f.tick();
    REQUIRE(f.world.ecs().get<RoleStatus>(f.monster).isPatrol());
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
        CHECK(f.world.ecs().get<RoleStatus>(f.monster).isPatrol());
        CHECK(f.world.ecs().get<Transform>(f.monster).position.x == goal.x);
        for (int i = 0; i < 20 && f.world.ecs().get<RoleStatus>(f.monster).isPatrol(); ++i) f.tick();
        CHECK_FALSE(f.world.ecs().get<RoleStatus>(f.monster).isPatrol());
        CHECK_FALSE(f.world.ecs().get<AiAgent>(f.monster).m_alerted);
    }
    SUBCASE("patrol discovers a target during dwell and alerts")
    {
        const auto target = f.create(false, 2, goal);
        f.tick();
        CHECK(f.world.ecs().get<SkillPool>(f.monster).m_target == target);
        CHECK(f.world.ecs().get<AiAgent>(f.monster).m_alerted);
    }
}

TEST_CASE("Ai monster hit recovery casts X and alerts, nonalert idle skill uses Z")
{
    MonsterFixture f;
    const auto target = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    f.load(true);
    auto& ecs = f.world.ecs();
    auto& pool = ecs.get<SkillPool>(f.monster);
    auto& agent = ecs.get<AiAgent>(f.monster);
    pool.m_target = target;
    SUBCASE("hit X path")
    {
        ecs.get<RoleStatus>(f.monster).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::Hit;
        ai::AiDriver(f.world, f.monster).update();
        CHECK(pool.m_current == kNoSkill);
        ecs.get<RoleStatus>(f.monster).eEntityStatus = EntityRoleStatus::Idle;
        ai::AiDriver(f.world, f.monster).addThreat(target, "1"_fx);
        ai::AiDriver(f.world, f.monster).update();
        REQUIRE(pool.m_current != kNoSkill);
        CHECK(pool.baseSlot(pool.m_skills[pool.m_current]) == "X");
        CHECK(ecs.get<RoleStatus>(f.monster).isAttack());
        CHECK(agent.m_alerted);
    }
    SUBCASE("nonalert idle skill uses Z")
    {
        agent.m_lastAction = ai::AiAction::Patrol;
        ai::AiDriver(f.world, f.monster).update();
        REQUIRE(pool.m_current != kNoSkill);
        CHECK(pool.baseSlot(pool.m_skills[pool.m_current]) == "Z");
    }
}

TEST_CASE("Ai monster Alert chases to target near-side bounds and wanders within range")
{
    MonsterFixture f;
    auto& ecs = f.world.ecs();
    const auto target = f.create(false, 2, {"200"_fx, {}, "100"_fx});
    ecs.get<SkillPool>(f.monster).m_target = target;
    ecs.get<AiAgent>(f.monster).m_alerted = true;
    f.tick();
    CHECK(ecs.get<RoleStatus>(f.monster).isChase());
    CHECK(ecs.get<RoleStatus>(f.monster).entityParams.move_pos.x == "160"_fx);
    CHECK(ecs.get<RoleStatus>(f.monster).entityParams.move_pos.z == "80"_fx);
    REQUIRE(f.movement());
    for (int i = 0; i < 120 && !f.movement()->m_complete; ++i) f.tick();
    REQUIRE(f.movement()->m_complete);
    CHECK(ecs.get<Transform>(f.monster).position.x == "160"_fx);
    CHECK(ecs.get<Transform>(f.monster).position.z == "80"_fx);
    for (int i = 0; i < 30 && !ecs.get<RoleStatus>(f.monster).isAlert(); ++i) f.tick();
    REQUIRE(ecs.get<RoleStatus>(f.monster).isAlert());
    const auto goal = ecs.get<RoleStatus>(f.monster).entityParams.move_pos;
    CHECK(goal.x >= "160"_fx); CHECK(goal.x <= "180"_fx);
    CHECK(goal.z >= "80"_fx); CHECK(goal.z <= "90"_fx);
}

TEST_CASE("Ai monster selects priority then index resets peers and consumes SkillAi gating")
{
    MonsterFixture f;
    const auto target = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    f.load();
    auto& ecs = f.world.ecs();
    auto& pool = ecs.get<SkillPool>(f.monster);
    auto& agent = ecs.get<AiAgent>(f.monster);
    pool.m_target = target;
    agent.m_alerted = true;
    agent.m_skillIntervalMs = {Fixed{}, Fixed{}, Fixed{}};
    ai::AiDriver(f.world, f.monster).update();
    REQUIRE(pool.m_current != kNoSkill);
    CHECK(pool.baseSlot(pool.m_skills[pool.m_current]) == "B");
    CHECK(agent.m_skillIntervalMs[0] == Fixed{});
    CHECK(agent.m_skillIntervalMs[1] == "200"_fx);
    CHECK(agent.m_skillIntervalMs[2] == "300"_fx);
    SUBCASE("SkillAi blocks first ready high priority slot")
    {
        const auto b = skill::getGivenSkill(pool, "B");
        REQUIRE(b != kNoSkill);
        idle(f.world, f.monster);
        pool.m_skills[b].m_ai.m_configId = f.archive.skill_ai.front().id;
        pool.m_skills[b].m_ai.m_loadCd = "1000"_fx;
        agent.m_skillIntervalMs = {Fixed{}, Fixed{}, Fixed{}};
        ai::AiDriver(f.world, f.monster).update();
        REQUIRE(pool.m_current != kNoSkill);
        CHECK(pool.baseSlot(pool.m_skills[pool.m_current]) == "C");
    }
    ai::AiDriver(f.world, f.monster).update();
    CHECK(agent.m_skillIntervalMs[1] < "200"_fx);
}

TEST_CASE("Ai monster retargets dead enemies and preserves Alert with no targets")
{
    MonsterFixture f;
    auto& ecs = f.world.ecs();
    const auto first = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    const auto second = f.create(false, 4, {"200"_fx, {}, "50"_fx});
    auto& agent = ecs.get<AiAgent>(f.monster);
    auto& pool = ecs.get<SkillPool>(f.monster);
    agent.m_alerted = true;
    pool.m_target = first;
    ecs.get<Vitals>(first).hp = {};
    ai::AiDriver(f.world, f.monster).update();
    CHECK(pool.m_target == second);
    ecs.get<Vitals>(second).hp = {};
    idle(f.world, f.monster);
    ai::AiDriver(f.world, f.monster).update();
    CHECK(pool.m_target == f.monster);
    CHECK(agent.m_alerted);
    CHECK_FALSE(ecs.get<RoleStatus>(f.monster).isPatrol());
    CHECK(ai::AiDriver(f.world, f.monster).candidates().empty());
}

TEST_CASE("Ai monster threat changes every ten seconds and taunt forces target before sight")
{
    MonsterFixture f;
    auto& ecs = f.world.ecs();
    const auto first = f.create(false, 2, {"100"_fx, {}, "50"_fx});
    const auto second = f.create(false, 4, {"200"_fx, {}, "50"_fx});
    ecs.get<SkillPool>(f.monster).m_target = first;
    ecs.get<AiAgent>(f.monster).m_alerted = true;
    ai::AiDriver(f.world, f.monster).addThreat(first, "10"_fx);
    ai::AiDriver(f.world, f.monster).addThreat(second, "20"_fx);
    ecs.get<AiAgent>(f.monster).m_changeTargetMs = "10000"_fx - ecs.singleton<WorldClock>().dtMs * "2"_fx;
    ai::AiDriver(f.world, f.monster).update();
    CHECK(ecs.get<SkillPool>(f.monster).m_target == first);
    ai::AiDriver(f.world, f.monster).update();
    CHECK(ecs.get<SkillPool>(f.monster).m_target == second);
    CHECK(ecs.get<AiAgent>(f.monster).m_threat.empty());
    ecs.get<RoleTimers>(f.monster).m_sneerRecounts = {{first, "100"_fx}, {second, "1"_fx}};
    REQUIRE(buff::BuffHolder(f.world, f.monster).sneerTarget() == first);
    const auto elapsed = ecs.get<AiAgent>(f.monster).m_changeTargetMs;
    ai::AiDriver(f.world, f.monster).update();
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
