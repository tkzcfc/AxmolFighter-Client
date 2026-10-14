#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffPool.h"
#include "mugen/sim/hit/HitSeams.h"

using namespace skill_test;

namespace
{
// 立即进入、命中自身、概率必过的爆发配置。
int32_t crazyBuffId()
{
    const auto& rows = conf::Config::instance().table<conf::BuffConfig>();
    const auto row   = std::find_if(rows.begin(), rows.end(), [](const conf::BuffConfig& value) {
        const auto* rule   = conf::Config::instance().find<conf::BuffRuleConfig>(value.ruleId);
        const bool open    = value.condition.empty() || value.condition.front() == 0;
        const bool anyArea = value.areaSetting.empty() || value.areaSetting.front() == -1;
        return rule && rule->className == "BuffCrazy" && value.executeType == 1 && value.target == 1 &&
               value.probability >= "100"_fx && open && anyArea;
    });
    REQUIRE(row != rows.end());
    return row->id;
}

// 零消耗技能。首次按键选中链头，不依赖后续段。
int32_t cheapSkill(int32_t skip)
{
    return findSkill([skip](const conf::SkillAttackConfig& row) {
        return row.id != skip && row.cd == 0 && row.mp <= 0 && row.ep <= 0 && row.crystal <= 0;
    });
}

// 组 1 第 1 槽为 A，组 2 同槽为 A+。
void loadNormalAndCrazy(World& world, Entity entity, int32_t normal, int32_t crazy)
{
    skill::SkillLoadout data(5);
    data[0].push_back({{{normal, -1}}});
    data[1].push_back({{{crazy, -1}}});
    skill::SkillCaster(world, entity).loadSkill(data);
}

void expectCurrent(World& world, Entity entity, std::string_view slot, int32_t configId)
{
    const auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(pool.m_current != kNoSkill);
    const auto& skill = pool.m_skills[pool.m_current];
    CHECK(skill::baseSlot(pool, skill) == slot);
    CHECK(skill.m_configId == configId);
    CHECK(pool.m_current == skill::getGivenSkill(pool, slot));
}
}  // namespace

TEST_CASE("Crazy entry through buff selects plus slot")
{
    loadConfig();
    const auto normal = cheapSkill(-1);
    const auto crazy  = cheapSkill(normal);
    const auto buffId = crazyBuffId();
    World world;
    const auto entity = actor(world);
    loadNormalAndCrazy(world, entity, normal, crazy);
    auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(skill::getGivenSkill(pool, "A") != kNoSkill);
    REQUIRE(skill::getGivenSkill(pool, "A+") != kNoSkill);
    CHECK_FALSE(pool.m_crazy);

    idle(world, entity);
    REQUIRE(skill::SkillCaster(world, entity).dealButtonEvent(ControlEvent::CTR_ATKA));
    expectCurrent(world, entity, "A", normal);
    CHECK_FALSE(pool.m_crazy);

    idle(world, entity);
    HitBuffApplication application;
    application.m_buffId = Fixed::fromInt(buffId);
    buff::dealBuff(world, entity, application);
    REQUIRE(buff::hasBuff(world, entity, buffId));
    CHECK(pool.m_crazy);

    REQUIRE(skill::SkillCaster(world, entity).dealButtonEvent(ControlEvent::CTR_ATKA));
    expectCurrent(world, entity, "A+", crazy);
    CHECK(pool.m_crazy);
}
