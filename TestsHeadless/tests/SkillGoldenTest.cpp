#include "SkillTestSupport.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>

using namespace skill_test;

TEST_CASE("Skill golden matches ordered slots and steps for two heroes and two monsters")
{
    loadConfig();
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path() / "golden/skillpool_golden.csv");
    REQUIRE(file.is_open());
    std::map<int32_t, std::vector<std::pair<std::string, int32_t>>> expected;
    std::string line;
    while (std::getline(file, line))
    {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream input(line);
        int32_t role, steps;
        std::string slot;
        REQUIRE(static_cast<bool>(input >> role >> slot >> steps));
        expected[role].emplace_back(slot, steps);
    }
    REQUIRE(expected.size() == 4);
    int heroes = 0, monsters = 0;
    for (const auto& [roleId, slots] : expected)
    {
        World world;
        const auto entity = actor(world);
        const auto& role = conf::Config::instance().get<conf::RoleConfig>(roleId);
        heroes += role.roleType == 1;
        monsters += role.roleType == 2;
        world.ecs().get<RoleIdentity>(entity).roleId = roleId;
        skill::SkillCaster(world, entity).loadSkill(skill::SkillAssembly::monsterLoadout(role.aiId.at(0)));
        const auto& pool = world.ecs().get<SkillPool>(entity);
        REQUIRE(pool.m_slots.size() == slots.size());
        for (size_t s = 0; s < slots.size(); ++s)
        {
            CHECK(pool.m_slots[s].m_name == slots[s].first);
            CHECK(std::count_if(pool.m_skills.begin(), pool.m_skills.end(), [s](const auto& k) {
                return k.m_address.m_slot == s;
            }) == slots[s].second);
        }
        ByteBuffer bytes;
        world.serialize(bytes);
        World restored;
        REQUIRE(restored.deserialize(bytes));
        CHECK(world.hash() == restored.hash());
    }
    CHECK(heroes == 2);
    CHECK(monsters == 2);
}

TEST_CASE("Skill player slot mapping rock and change-system expansion use configuration")
{
    loadConfig();
    const auto id = findSkill([](const auto& r) { return r.nextSkill == -1; });
    const auto& rocks = conf::Config::instance().table<conf::SkillRockConfig>();
    const auto rock = std::find_if(rocks.begin(), rocks.end(), [](const auto& r) {
        return r.commandIndex > 0 && conf::Config::instance().find<conf::SkillAttackConfig>(r.skillId);
    });
    REQUIRE(rock != rocks.end());
    World world;
    const auto entity = actor(world);
    skill::SkillCaster(world, entity).loadSkill(skill::SkillAssembly::playerLoadout({{EnumSkillSlot::Attack, id},
        {EnumSkillSlot::Sprint, id}, {EnumSkillSlot::A, id, rock->id}, {EnumSkillSlot::SA, -1, rock->id}}));
    auto& pool = world.ecs().get<SkillPool>(entity);
    CHECK(skill::getGivenSkill(pool, "A") != kNoSkill);
    CHECK(skill::getGivenSkill(pool, "A+") != kNoSkill);
    CHECK(skill::getGivenSkill(pool, "Y") != kNoSkill);
    CHECK(skill::getGivenSkill(pool, "B", 1, 2) != kNoSkill);
    CHECK(skill::getGivenSkill(pool, std::to_string(rock->commandIndex) + '+') != kNoSkill);
    const auto& changes = conf::Config::instance().table<conf::SkillChangeSystemConfig>();
    const auto change = std::find_if(changes.begin(), changes.end(), [](const auto& r) {
        return r.skillChange > 2 && conf::Config::instance().find<conf::SkillAttackConfig>(r.id) &&
               conf::Config::instance().find<conf::SkillAttackConfig>(r.skillNewId);
    });
    REQUIRE(change != changes.end());
    skill::SkillCaster(world, entity).loadSkill(skill::SkillAssembly::playerLoadout({{EnumSkillSlot::Attack, change->id}}));
    const auto& changed = world.ecs().get<SkillPool>(entity);
    const auto index = skill::getGivenSkill(changed, "A", 1, change->skillChange);
    REQUIRE(index != kNoSkill);
    CHECK(changed.m_skills[index].m_configId == change->skillNewId);
}
