#include "SkillTestSupport.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include <algorithm>
#include <variant>

using namespace skill_test;

TEST_CASE("PlayerKit mage spawn loads profession tree skill 980000")
{
    loadConfig();
    World world;
    RoleSpawnParams params;
    params.roleId     = 104;
    params.playerSlot = 0;
    const auto entity = RoleSpawner(world).spawnRole(params);
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
    CHECK(skill::SkillAssembly::eventSlot(ControlEvent::CTR_ATKA) == "A");
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
    const auto entity = RoleSpawner(world).spawnRole(params);
    const auto& identity = world.ecs().get<RoleIdentity>(entity);
    CHECK(identity.roleId == 0);
    const auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(!pool.m_slots.empty());
    REQUIRE(!pool.m_skills.empty());

    World scratch;
    const auto expectedEntity = actor(scratch);
    scratch.ecs().get<RoleIdentity>(expectedEntity).roleId = 0;
    skill::SkillCaster(scratch, expectedEntity).loadSkill(skill::SkillAssembly::monsterLoadout(identity.aiId));
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
    const auto entity = RoleSpawner(world).spawnRole(params);
    CHECK(world.ecs().tryGet<AiAgent>(entity) != nullptr);
    const auto& identity = world.ecs().get<RoleIdentity>(entity);
    const auto& pool     = world.ecs().get<SkillPool>(entity);
    REQUIRE(!pool.m_skills.empty());

    World scratch;
    const auto expectedEntity = actor(scratch);
    scratch.ecs().get<RoleIdentity>(expectedEntity).roleId = 261;
    skill::SkillCaster(scratch, expectedEntity).loadSkill(skill::SkillAssembly::monsterLoadout(identity.aiId));
    const auto& expected = scratch.ecs().get<SkillPool>(expectedEntity);
    REQUIRE(!expected.m_skills.empty());
    CHECK(pool.m_skills.size() == expected.m_skills.size());
    CHECK(pool.m_skills.front().m_configId == expected.m_skills.front().m_configId);
}

TEST_CASE("PlayerKit fighter occupation returns empty kit")
{
    loadConfig();
    const auto kit = skill::SkillAssembly::offlineKit(CharacterClass::kFighter);
    CHECK(kit.empty());
}

TEST_CASE("PlayerKit swordman and ranger trees produce leaf attack skills")
{
    loadConfig();
    const auto sword = skill::SkillAssembly::offlineKit(CharacterClass::kSwordman);
    REQUIRE(!sword.empty());
    CHECK(sword.front().m_slot == EnumSkillSlot::Attack);
    CHECK(sword.front().m_baseId == 920000);
    const auto ranger = skill::SkillAssembly::offlineKit(CharacterClass::kRanger);
    REQUIRE(!ranger.empty());
    CHECK(ranger.front().m_slot == EnumSkillSlot::Attack);
    CHECK(ranger.front().m_baseId == 930030);
}

TEST_CASE("PlayerKit partner professions keep source slot mapping")
{
    loadConfig();
    struct Expect
    {
        CharacterClass cls;
        std::vector<std::pair<EnumSkillSlot, int32_t>> slots;
    };
    const std::vector<Expect> expects = {
        {CharacterClass::kPartnerWind,
         {{EnumSkillSlot::Attack, 44000}, {EnumSkillSlot::Sprint, 44010}, {EnumSkillSlot::Dodge, 44020},
          {EnumSkillSlot::A, 44040},      {EnumSkillSlot::B, 44060},      {EnumSkillSlot::C, 44080},
          {EnumSkillSlot::SA, 44110},     {EnumSkillSlot::SB, 44120},     {EnumSkillSlot::SC, 44130},
          {EnumSkillSlot::EA, 44102},     {EnumSkillSlot::Break, 44140}}},
        {CharacterClass::kPartnerQueen,
         {{EnumSkillSlot::Attack, 46000}, {EnumSkillSlot::Sprint, 46010}, {EnumSkillSlot::Dodge, 46020},
          {EnumSkillSlot::A, 46040},      {EnumSkillSlot::B, 46060},      {EnumSkillSlot::C, 46080},
          {EnumSkillSlot::SA, 46110},     {EnumSkillSlot::SB, 46120},     {EnumSkillSlot::SC, 46130},
          {EnumSkillSlot::EA, 46102},     {EnumSkillSlot::Break, 46140}}},
        {CharacterClass::kPartnerRage,
         {{EnumSkillSlot::Attack, 48000}, {EnumSkillSlot::Sprint, 48010}, {EnumSkillSlot::Dodge, 48020},
          {EnumSkillSlot::A, 48040},      {EnumSkillSlot::B, 48060},      {EnumSkillSlot::C, 48080},
          {EnumSkillSlot::SA, 48110},     {EnumSkillSlot::SB, 48120},     {EnumSkillSlot::SC, 48130},
          {EnumSkillSlot::EA, 48102},     {EnumSkillSlot::Break, 48140},  {EnumSkillSlot::Anger, 48150}}},
    };

    for (const auto& expect : expects)
    {
        const auto rows = skill::SkillAssembly::offlineKitRows(expect.cls);
        REQUIRE(!rows.empty());
        for (const auto& [slot, leafId] : expect.slots)
        {
            const auto it = std::find_if(rows.begin(), rows.end(), [slot](const auto& row) { return row.slot == slot; });
            REQUIRE(it != rows.end());
            CHECK(it->leafId == leafId);
            const auto* leaf = conf::Config::instance().find<conf::SkillLeafConfig>(leafId);
            REQUIRE(leaf != nullptr);
            CHECK(it->skillId == leaf->skillId);
        }
    }
}

TEST_CASE("PlayerKit partner role ids spawn via player path")
{
    loadConfig();
    const std::vector<std::pair<CharacterClass, int32_t>> expects = {
        {CharacterClass::kPartnerWind, 105},
        {CharacterClass::kPartnerQueen, 106},
        {CharacterClass::kPartnerRage, 107},
    };
    for (const auto& [occupation, roleId] : expects)
    {
        World world;
        RoleSpawnParams params;
        params.playerSlot = 0;
        const auto entity = RoleSpawner(world).spawnPlayer(occupation, params);
        REQUIRE(entity.value != 0);
        const auto& identity = world.ecs().get<RoleIdentity>(entity);
        CHECK(identity.roleId == roleId);
        const auto& pool = world.ecs().get<SkillPool>(entity);
        const auto attack = skill::getGivenSkill(pool, "A");
        CHECK(attack != kNoSkill);
    }
}
