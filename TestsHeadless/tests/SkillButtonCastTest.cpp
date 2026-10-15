#include "SkillTestSupport.h"

#include <algorithm>
#include <vector>

using namespace skill_test;

namespace
{
// dealWithButton 之后当前技能应落在的槽位、配置和模式。
struct ExpectedCast
{
    std::string_view m_slot;
    int32_t m_configId = 0;
    int32_t m_mode     = 1;
};

// 一次按键：事件加上期望选中的技能。
struct ButtonPress
{
    ControlEvent m_event;
    ExpectedCast m_expect;
};

// 攻击与摇杆强化技能。强化 id 为 -1 表示该槽缺失。
struct RockLoad
{
    int32_t m_attack = -1;
    int32_t m_rock   = -1;
};

// 零消耗技能。已用 id 不再重复，槽位断言才能区分配置。
int32_t takeCheap(std::vector<int32_t>& used)
{
    const auto id = findSkill([&](const conf::SkillAttackConfig& row) {
        return row.cd == 0 && row.mp <= 0 && row.ep <= 0 && row.crystal <= 0 &&
               std::find(used.begin(), used.end(), row.id) == used.end();
    });
    used.push_back(id);
    return id;
}

void grantCrystals(World& world, Entity entity)
{
    world.ecs().get<SkillPool>(entity).m_crystals = 1000000;
}

// 组 1 按顺序是 A B C D。空链会被跳过，所以必须依次压入。
struct LetterIds
{
    int32_t m_b = 0;
    int32_t m_c = 0;
    int32_t m_d = 0;
};

LetterIds loadLetters(World& world, Entity entity)
{
    std::vector<int32_t> used;
    const auto attack = takeCheap(used);
    const LetterIds ids{takeCheap(used), takeCheap(used), takeCheap(used)};
    skill::SkillLoadout data(5);
    data[0].push_back({{{attack, -1}}});
    data[0].push_back({{{ids.m_b, -1}}});
    data[0].push_back({{{ids.m_c, -1}}});
    data[0].push_back({{{ids.m_d, -1}}});
    skill::SkillCaster(world, entity).loadSkill(data);
    grantCrystals(world, entity);
    return ids;
}

// 组 1 的 B/C/D 各有模式 1 和模式 2。模式 2 对应事件槽位名里的 _2。
struct SecondIds
{
    int32_t m_b2 = 0;
    int32_t m_c2 = 0;
    int32_t m_d2 = 0;
};

SecondIds loadSecondForm(World& world, Entity entity)
{
    std::vector<int32_t> used;
    const auto attack = takeCheap(used);
    const auto baseB  = takeCheap(used);
    const auto baseC  = takeCheap(used);
    const auto baseD  = takeCheap(used);
    const SecondIds ids{takeCheap(used), takeCheap(used), takeCheap(used)};
    skill::SkillLoadout data(5);
    data[0].push_back({{{attack, -1}}});
    data[0].push_back({{{baseB, -1}, {ids.m_b2, -1}}});
    data[0].push_back({{{baseC, -1}, {ids.m_c2, -1}}});
    data[0].push_back({{{baseD, -1}, {ids.m_d2, -1}}});
    skill::SkillCaster(world, entity).loadSkill(data);
    grantCrystals(world, entity);
    return ids;
}

// 组 1 第 1 槽为 A。强化技能放在组 3 第 1 槽，槽位名为 1+。
void loadRock(World& world, Entity entity, RockLoad ids)
{
    skill::SkillLoadout data(5);
    data[0].push_back({{{ids.m_attack, -1}}});
    if (ids.m_rock >= 0)
    {
        data[2].resize(1);
        data[2][0].push_back({{ids.m_rock, -1}});
    }
    skill::SkillCaster(world, entity).loadSkill(data);
    grantCrystals(world, entity);
}

void expectCurrent(World& world, Entity entity, ExpectedCast expected)
{
    const auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(pool.m_current != kNoSkill);
    const auto& current = pool.m_skills[pool.m_current];
    CHECK(pool.baseSlot(current) == expected.m_slot);
    CHECK(current.m_configId == expected.m_configId);
    CHECK(current.m_address.m_mode == expected.m_mode);
}

void press(World& world, Entity entity, ButtonPress button)
{
    idle(world, entity);
    CHECK_FALSE(world.ecs().get<SkillPool>(entity).m_crazy);
    REQUIRE(skill::SkillCaster(world, entity).dealWithButton(button.m_event));
    expectCurrent(world, entity, button.m_expect);
}
}  // namespace

TEST_CASE("SkillButton attack B C D selects those slots through dealWithButton")
{
    loadConfig();
    World world;
    const auto entity = actor(world);
    const auto ids    = loadLetters(world, entity);
    press(world, entity, {ControlEvent::CTR_ATKB, {"B", ids.m_b, 1}});
    press(world, entity, {ControlEvent::CTR_ATKC, {"C", ids.m_c, 1}});
    press(world, entity, {ControlEvent::CTR_ATKD, {"D", ids.m_d, 1}});
}

TEST_CASE("SkillButton joy B C D selects mode 2 through dealWithButton")
{
    loadConfig();
    World world;
    const auto entity = actor(world);
    const auto ids    = loadSecondForm(world, entity);
    press(world, entity, {ControlEvent::CTR_JOYB, {"B", ids.m_b2, 2}});
    press(world, entity, {ControlEvent::CTR_JOYC, {"C", ids.m_c2, 2}});
    press(world, entity, {ControlEvent::CTR_JOYD, {"D", ids.m_d2, 2}});
}

TEST_CASE("SkillButton JOY1 selects the rock skill through dealWithButton")
{
    loadConfig();
    std::vector<int32_t> used;
    const RockLoad ids{takeCheap(used), takeCheap(used)};
    World world;
    const auto entity = actor(world);
    loadRock(world, entity, ids);
    const auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(skill::SkillAssembly::eventSlot(ControlEvent::CTR_JOY1) == "1+");
    REQUIRE(skill::getGivenSkill(pool, "1+") != kNoSkill);
    // 下、左、上、右、下再接攻击 A 之后，输入层发出 CTR_JOY1。
    press(world, entity, {ControlEvent::CTR_JOY1, {"1+", ids.m_rock, 1}});
}

TEST_CASE("SkillButton JOY1 falls back to A when the rock skill is absent")
{
    loadConfig();
    std::vector<int32_t> used;
    const RockLoad ids{takeCheap(used), -1};
    World world;
    const auto entity = actor(world);
    loadRock(world, entity, ids);
    REQUIRE(skill::getGivenSkill(world.ecs().get<SkillPool>(entity), "1+") == kNoSkill);
    press(world, entity, {ControlEvent::CTR_JOY1, {"A", ids.m_attack, 1}});
}
