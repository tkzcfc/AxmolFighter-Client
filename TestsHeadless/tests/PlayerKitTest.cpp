#include "SkillTestSupport.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include <variant>

using namespace skill_test;

TEST_CASE("PlayerKit mage spawn loads profession tree skill 980000")
{
    loadConfig();
    World world;
    RoleSpawnParams params;
    params.roleId     = 104;
    params.playerSlot = 0;
    const auto entity = spawnRole(world, params);
    const auto& pool  = world.ecs().get<SkillPool>(entity);
    const auto index  = skill::getGivenSkill(pool, "A");
    REQUIRE(index != kNoSkill);
    CHECK(pool.m_skills[index].m_configId == 980000);
    CHECK(pool.m_skills[index].m_configId != 82010);

    const auto sprint = skill::getGivenSkill(pool, "Y");
    REQUIRE(sprint != kNoSkill);
    const auto* leaf = conf::Config::instance().find<conf::SkillLeafConfig>(40020);
    REQUIRE(leaf != nullptr);
    CHECK(pool.m_skills[sprint].m_configId == leaf->skillId);

    const auto sub = skill::getGivenSkill(pool, "J");
    REQUIRE(sub != kNoSkill);
    const auto* buffLeaf = conf::Config::instance().find<conf::SkillLeafConfig>(40090);
    REQUIRE(buffLeaf != nullptr);
    CHECK(pool.m_skills[sub].m_configId == buffLeaf->skillId);

    const auto sap = skill::getGivenSkill(pool, "M");
    REQUIRE(sap != kNoSkill);
    CHECK(pool.m_skills[sap].m_configId == 980120);
}

TEST_CASE("PlayerKit mage attack input maps to action 10100")
{
    loadConfig();
    CHECK(skill::eventSlot(ControlEvent::CTR_ATKA) == "A");
    const auto* skill = conf::Config::instance().find<conf::SkillAttackConfig>(980000);
    REQUIRE(skill != nullptr);
    REQUIRE(!skill->actionIds.empty());
    REQUIRE(!skill->actionIds.front().empty());
    const auto* action =
        conf::Config::instance().find<conf::ActionConfig>(skill->actionIds.front().front());
    REQUIRE(action != nullptr);
    CHECK(std::get<int32_t>(action->action) == 10100);
}

TEST_CASE("PlayerKit harness role zero keeps monster kit")
{
    loadConfig();
    World world;
    RoleSpawnParams params;
    params.roleId     = 0;
    params.playerSlot = 0;
    const auto entity = spawnRole(world, params);
    const auto& identity = world.ecs().get<RoleIdentity>(entity);
    CHECK(identity.roleId == 0);
    const auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(!pool.m_slots.empty());
    REQUIRE(!pool.m_skills.empty());

    World scratch;
    const auto expectedEntity = actor(scratch);
    scratch.ecs().get<RoleIdentity>(expectedEntity).roleId = 0;
    skill::loadSkill(scratch, expectedEntity, skill::monsterLoadout(identity.aiId));
    const auto& expected = scratch.ecs().get<SkillPool>(expectedEntity);
    REQUIRE(!expected.m_skills.empty());
    CHECK(pool.m_skills.size() == expected.m_skills.size());
    CHECK(pool.m_skills.front().m_configId == expected.m_skills.front().m_configId);
    CHECK(pool.m_slots.size() == expected.m_slots.size());
}

TEST_CASE("PlayerKit monster spawn keeps AI skill ids")
{
    loadConfig();
    World world;
    RoleSpawnParams params;
    params.roleId = 261;
    params.isAi   = true;
    const auto entity = spawnRole(world, params);
    CHECK(world.ecs().tryGet<AiAgent>(entity) != nullptr);
    const auto& identity = world.ecs().get<RoleIdentity>(entity);
    const auto& pool     = world.ecs().get<SkillPool>(entity);
    REQUIRE(!pool.m_skills.empty());

    World scratch;
    const auto expectedEntity = actor(scratch);
    scratch.ecs().get<RoleIdentity>(expectedEntity).roleId = 261;
    skill::loadSkill(scratch, expectedEntity, skill::monsterLoadout(identity.aiId));
    const auto& expected = scratch.ecs().get<SkillPool>(expectedEntity);
    REQUIRE(!expected.m_skills.empty());
    CHECK(pool.m_skills.size() == expected.m_skills.size());
    CHECK(pool.m_skills.front().m_configId == expected.m_skills.front().m_configId);
}

TEST_CASE("PlayerKit fighter occupation returns empty kit")
{
    loadConfig();
    const auto kit = skill::offlineKit(CharacterClass::kFighter);
    CHECK(kit.empty());
}

TEST_CASE("PlayerKit swordman and ranger trees produce leaf attack skills")
{
    loadConfig();
    const auto sword = skill::offlineKit(CharacterClass::kSwordman);
    REQUIRE(!sword.empty());
    CHECK(sword.front().m_slot == EnumSkillSlot::Attack);
    CHECK(sword.front().m_baseId == 920000);
    const auto ranger = skill::offlineKit(CharacterClass::kRanger);
    REQUIRE(!ranger.empty());
    CHECK(ranger.front().m_slot == EnumSkillSlot::Attack);
    CHECK(ranger.front().m_baseId == 930030);
}
