#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/role/brain/states/HitStates.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace mugen;
using namespace mugen::sim;

// 表征测试：固定疯狂（E 槽 / crazy）模式与技能槽位命名行为。
namespace
{
// 构建装配了指定职业完整离线技能的角色，并补齐行为树所需组件。
Entity fullActor(World& world, CharacterClass occupation = CharacterClass::kSwordman)
{
    skill_test::loadConfig();
    const auto entity = skill_test::actor(world);
    skill::loadSkill(world, entity, skill::playerLoadout(skill::offlineKit(occupation)));
    world.ecs().add(entity, MotionState{});
    world.ecs().add(entity, RoleHitState{});
    world.ecs().add(entity, RoleTimers{});
    world.ecs().add(entity, Displacement{});
    world.ecs().add(entity, ColliderState{});
    world.ecs().add(entity, RoleBrain{});
    world.ecs().add(entity, RoleControl{});
    return entity;
}
void reloadConfig()
{
    conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin");
}
}  // namespace

TEST_CASE("Crazy slot rejects when EP below epCost and emits body-shake tip")
{
    World world;
    const auto entity = fullActor(world);
    auto& pool        = world.ecs().get<SkillPool>(entity);
    int32_t crazyIdx  = kNoSkill;
    for (int32_t i = 0; i < static_cast<int32_t>(pool.m_skills.size()); ++i)
        if (skill::epCost(pool, pool.m_skills[static_cast<size_t>(i)]) > Fixed{})
        {
            crazyIdx = i;
            break;
        }
    REQUIRE(crazyIdx != kNoSkill);
    const auto cost = skill::epCost(pool, pool.m_skills[static_cast<size_t>(crazyIdx)]);
    CHECK(cost == Fixed::fromInt(
                      conf::Config::instance().get<conf::SkillAttackConfig>(pool.m_skills[static_cast<size_t>(crazyIdx)].m_configId).ep));
    auto& vitals            = world.ecs().get<Vitals>(entity);
    vitals.ep               = cost - "1"_fx;
    auto& events            = world.ecs().singleton<PresentationEvents>();
    const auto beforeEvents = events.events.size();
    CHECK_FALSE(skill::isAllowCast(world, entity, crazyIdx));
    CHECK(events.events.size() == beforeEvents + 1);
    vitals.ep = cost;
    CHECK(skill::isAllowCastInternal(world, entity, crazyIdx));
    reloadConfig();
}

TEST_CASE("castBegan skips MP and EP deduction while crazy")
{
    World world;
    const auto entity = fullActor(world);
    auto& pool        = world.ecs().get<SkillPool>(entity);
    int32_t mpIdx     = kNoSkill;
    for (int32_t i = 0; i < static_cast<int32_t>(pool.m_skills.size()); ++i)
        if (skill::mpCost(pool, pool.m_skills[static_cast<size_t>(i)]) > Fixed{})
        {
            mpIdx = i;
            break;
        }
    REQUIRE(mpIdx != kNoSkill);
    auto& vitals = world.ecs().get<Vitals>(entity);
    vitals.mp    = "100"_fx;
    vitals.ep    = "100"_fx;
    pool.m_crazy = false;
    skill::castBegan(world, entity, mpIdx);
    CHECK(vitals.mp < "100"_fx);
    pool.m_crazy = true;
    vitals.mp    = "100"_fx;
    vitals.ep    = "100"_fx;
    skill::castBegan(world, entity, mpIdx);
    CHECK(vitals.mp == "100"_fx);
    CHECK(vitals.ep == "100"_fx);
    reloadConfig();
}

TEST_CASE("E slot rejected again at key entry while already crazy")
{
    World world;
    const auto entity = fullActor(world);
    auto& pool        = world.ecs().get<SkillPool>(entity);
    pool.m_crazy      = true;
    skill::resetSkill(world, entity);
    const auto before = pool.m_current;
    CHECK_FALSE(skill::dealWithButton(world, entity, ControlEvent::CTR_ATKE));
    CHECK(pool.m_current == before);
    reloadConfig();
}

TEST_CASE("A input becomes A+ while crazy")
{
    World world;
    const auto entity = fullActor(world);
    auto& pool        = world.ecs().get<SkillPool>(entity);
    pool.m_crazy      = true;
    skill::resetSkill(world, entity);
    REQUIRE(skill::dealWithButton(world, entity, ControlEvent::CTR_ATKA));
    REQUIRE(pool.m_current != kNoSkill);
    CHECK(skill::baseSlot(pool, pool.m_skills[static_cast<size_t>(pool.m_current)]) == "A+");
    reloadConfig();
}

TEST_CASE("Death during crazy zeroes EP and exits crazy")
{
    World world;
    const auto entity = fullActor(world);
    auto& pool        = world.ecs().get<SkillPool>(entity);
    pool.m_crazy      = true;
    pool.m_slotChanged = true;
    world.ecs().get<Vitals>(entity).ep = "500"_fx;
    role::brain::RoleContext rc{world, entity};
    behavior::HitActionState state;
    role::brain::states::HitState::enter(rc, behavior::ActionType::Death, state);
    CHECK(world.ecs().get<Vitals>(entity).ep == Fixed{});
    CHECK_FALSE(pool.m_crazy);
    reloadConfig();
}

TEST_CASE("Interrupt window advances current skill and rotates pipe")
{
    World world;
    const auto entity = fullActor(world);
    auto& pool        = world.ecs().get<SkillPool>(entity);
    int32_t chain     = kNoSkill;
    for (int32_t i = 0; i < static_cast<int32_t>(pool.m_skills.size()); ++i)
    {
        const auto* row =
            conf::Config::instance().find<conf::SkillAttackConfig>(pool.m_skills[static_cast<size_t>(i)].m_configId);
        if (row && row->nextSkill != -1 && conf::Config::instance().find<conf::SkillAttackConfig>(row->nextSkill))
        {
            chain = i;
            break;
        }
    }
    REQUIRE(chain != kNoSkill);
    auto& first           = pool.m_skills[static_cast<size_t>(chain)];
    const auto pipeBefore = first.m_pipe;
    pool.m_current        = chain;
    skill::castBegan(world, entity, chain);
    CHECK(first.m_previousPipe == pipeBefore);
    first.m_nextSkill = chain + 1;
    CHECK(skill::dealWithNextSkillBase(world, entity));
    CHECK(pool.m_current == chain + 1);
    reloadConfig();
}

TEST_CASE("Skill slot-name round trip matches golden list")
{
    skill_test::loadConfig();
    std::set<std::string> names;
    for (int occ = 1; occ <= 4; ++occ)
    {
        const auto skills = skill::offlineKit(static_cast<CharacterClass>(occ));
        if (skills.empty())
            continue;
        World world;
        const auto entity = skill_test::actor(world);
        skill::loadSkill(world, entity, skill::playerLoadout(skills));
        for (const auto& slot : world.ecs().get<SkillPool>(entity).m_slots)
            names.insert(slot.m_name);
    }
    for (int i = static_cast<int>(ControlEvent::CTR_JOY1); i <= static_cast<int>(ControlEvent::CTR_JOY12); ++i)
        names.insert(skill::eventSlot(static_cast<ControlEvent>(i)));
    for (int i = static_cast<int>(ControlEvent::CTR_JOYB); i <= static_cast<int>(ControlEvent::CTR_JOYD); ++i)
        names.insert(skill::eventSlot(static_cast<ControlEvent>(i)));
    std::string actual;
    for (const auto& name : names)
    {
        if (!actual.empty())
            actual += '\n';
        actual += name;
    }
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "data/slot_names.golden";
    std::ifstream in(path);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string golden = buffer.str();
    while (!golden.empty() && golden.back() == '\n')
        golden.pop_back();
    CHECK(actual == golden);
    reloadConfig();
}
