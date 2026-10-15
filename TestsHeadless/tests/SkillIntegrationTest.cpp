#include "SkillTestSupport.h"
#include "mugen/sim/spawn/RoleSpawn.h"

using namespace skill_test;

TEST_CASE("Skill cooldown advances before frozen role input and survives snapshot")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.cd > 100 && r.cd < 1000000; });
    const auto& roles = conf::Config::instance().table<conf::RoleConfig>();
    const auto role = std::find_if(roles.begin(), roles.end(), [](const auto& r) { return r.roleType == 1; });
    REQUIRE(role != roles.end());
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId = role->id;
    params.playerSlot = 0;
    const auto entity = spawnRole(world, params);
    slots(world, entity, {id});
    world.ecs().get<Vitals>(entity).mp = "100"_fx;
    auto& pool = world.ecs().get<SkillPool>(entity);
    const auto index = skill::getGivenSkill(pool, "A");
    skill::SkillCaster(world, entity).castBegan(index);
    const auto before = pool.m_skills[index].m_cd;
    world.ecs().get<RoleTimers>(entity).freezeMs = "100"_fx;
    FrameInput input;
    input.frame = 1;
    input.players.push_back({0, {{ControlEvent::CTR_ATKA, 0, 0, 1}}});
    world.tick(input);
    CHECK(pool.m_skills[index].m_cd == before - kLogicDtMs);
    CHECK(pool.m_current == kNoSkill);
    CHECK(world.ecs().get<ControlEventQueue>(entity).commands.size() == 1);
    ByteBuffer bytes;
    world.serialize(bytes);
    World restored;
    REQUIRE(restored.deserialize(bytes));
    world.tick({2, {}, {}, {}});
    restored.tick({2, {}, {}, {}});
    CHECK(world.hash() == restored.hash());
}

TEST_CASE("Skill FrameInput reaches attack control seam and resource hint")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.mp > 0 && r.ep > 0 && r.crystal <= 0; });
    const auto& roles = conf::Config::instance().table<conf::RoleConfig>();
    const auto role = std::find_if(roles.begin(), roles.end(), [](const auto& r) { return r.roleType == 1; });
    REQUIRE(role != roles.end());
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId = role->id;
    params.playerSlot = 0;
    const auto entity = spawnRole(world, params);
    slots(world, entity, {id});
    world.ecs().get<Vitals>(entity).ep = {};
    FrameInput input;
    input.frame = 1;
    input.players.push_back({0, {{ControlEvent::CTR_ATKA, 0, 0, 1}}});
    world.tick(input);
    CHECK(world.ecs().get<SkillPool>(entity).m_current == kNoSkill);
    const auto& events = world.ecs().singleton<PresentationEvents>().events;
    CHECK(std::any_of(events.begin(), events.end(), [](const auto& e) {
        const auto* tips = std::get_if<Tips>(&e.payload);
        return tips && tips->text == "body shake hint";
    }));
}

TEST_CASE("Skill cast synchronizes modes and forced interrupt clears cached next")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.cd > 0 && r.cd < 1000000; });
    World world;
    const auto entity = actor(world);
    skill::SkillLoadout data(5);
    data[0] = {{{{id, -1}, {id, -1}}}};
    skill::SkillCaster(world, entity).loadSkill(data);
    auto& pool = world.ecs().get<SkillPool>(entity);
    pool.m_crystals = 1000000;
    const auto normal = skill::getGivenSkill(pool, "A");
    const auto alternate = skill::getGivenSkill(pool, "A", 1, 2);
    REQUIRE(alternate != kNoSkill);
    pool.setSkillBase(normal);
    skill::SkillCaster(world, entity).castBegan(normal);
    CHECK(pool.m_skills[normal].m_cd == pool.m_skills[alternate].m_cd);
    CHECK(pool.m_skills[normal].m_releaseCount == pool.m_skills[alternate].m_releaseCount);
    pool.m_skills[normal].m_nextSkill = alternate;
    skill::SkillCaster(world, entity).forcedInterrupt();
    CHECK(pool.m_current == kNoSkill);
    CHECK(pool.m_skills[normal].m_nextSkill == kNoSkill);
}
