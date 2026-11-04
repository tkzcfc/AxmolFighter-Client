#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/buff/BuffPool.h"
#include "mugen/sim/buff/BuffHolder.h"
#include "mugen/sim/Singletons.h"

#include <array>

using namespace skill_test;

namespace
{
// 空标记规则枚举清单：本身无生命周期行为，仅在条件或免疫判定中按身份查询。
constexpr std::array markers{
    mugen::conf::BuffRuleId::BuffAdjudication, mugen::conf::BuffRuleId::BuffDark,
    mugen::conf::BuffRuleId::BuffDarkMark,     mugen::conf::BuffRuleId::BuffDarkMark2,
    mugen::conf::BuffRuleId::BuffThunder1,     mugen::conf::BuffRuleId::BuffThunder2,
    mugen::conf::BuffRuleId::BuffDeathMark,    mugen::conf::BuffRuleId::BuffLightMark,
    mugen::conf::BuffRuleId::BuffStoneSkin,    mugen::conf::BuffRuleId::BuffUnstable,
    mugen::conf::BuffRuleId::BuffReverse,      mugen::conf::BuffRuleId::BuffMight,
    mugen::conf::BuffRuleId::BuffCutSakura,    mugen::conf::BuffRuleId::BuffElectricEye,
    mugen::conf::BuffRuleId::BuffSkyProtect,   mugen::conf::BuffRuleId::BuffImmuneDebuff,
    mugen::conf::BuffRuleId::BuffElectron2,    mugen::conf::BuffRuleId::BuffSign,
};
}  // namespace

// 空标记规则已按空行为移植，既不是未移植占位，也不会触发未移植告警。
TEST_CASE("BuffRuleAudit marker rules are ported no-ops and never warn")
{
    using namespace mugen;
    using namespace mugen::sim;
    loadConfig();
    World world;
    const auto entity = actor(world);
    world.ecs().add(entity, BuffPool{});
    for (const auto rule : markers)
    {
        CHECK_FALSE(buff::isUnportedRule(rule));
        const auto& dispatch = buff::ruleDispatch(rule);
        for (const auto& hook : dispatch.m_hooks)
            hook(world, entity, 1, {});
        CHECK(world.ecs().singleton<mugen::sim::RoomState>().m_unportedBuffIds.empty());
    }
}

// 减益免疫仍按规则身份拒绝第三、四类减益的准入。
TEST_CASE("BuffRuleAudit ImmuneDebuff rejects debuff admission")
{
    using namespace mugen;
    using namespace mugen::sim;
    loadConfig();
    World world;
    const auto entity = actor(world);
    world.ecs().add(entity, BuffPool{});
    const auto& config = conf::Config::instance();
    int32_t immunityId = -1;
    for (const auto& row : config.table<conf::BuffConfig>())
    {
        const auto* rule = config.find<conf::BuffRuleConfig>(row.ruleId);
        if (rule && rule->className == "BuffImmuneDebuff")
        {
            immunityId = row.id;
            break;
        }
    }
    REQUIRE(immunityId != -1);
    BuffInstance instance;
    instance.m_id = immunityId;
    world.ecs().get<BuffPool>(entity).m_instances.push_back(instance);
    conf::BuffConfig candidate;
    candidate.areaSetting = {-1};
    for (int32_t type : {1, 2, 3, 4})
    {
        candidate.buffType = type;
        CHECK(buff::BuffHolder(world, entity).buffCheck(candidate, 0) == (type < 3));
    }
}
