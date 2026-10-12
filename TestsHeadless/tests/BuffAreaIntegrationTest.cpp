#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffPool.h"

// 使用实际限制场景的配置证明生产施加入口读取世界战斗类型。
TEST_CASE("BuffAreaIntegration world fight type controls production admission")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    const auto& rows = conf::Config::instance().table<conf::BuffConfig>();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const auto& value) {
        return value.areaSetting.size() > 1 && value.areaSetting[0] == 1 && value.probability >= "100"_fx &&
               value.executeType == 3 && value.began != static_cast<int>(BFEvent::AddBuff) &&
               conf::Config::instance().find<conf::BuffRuleConfig>(value.ruleId);
    });
    REQUIRE(row != rows.end());
    for (const bool allowed : {false, true})
    {
        World world;
        WorldSetup setup;
        setup.m_fightType = allowed ? row->areaSetting[1] : -999;
        world.init(setup);
        const auto entity = world.create(GroupKind::Role);
        world.ecs().add(entity, RoleIdentity{});
        world.ecs().add(entity, Vitals{});
        HitBuffApplication request;
        request.m_buffId = Fixed::fromInt(row->id);
        buff::dealBuff(world, entity, request);
        CHECK(buff::hasBuff(world, entity, row->id) == allowed);
        CHECK(world.clone().setup().m_fightType == setup.m_fightType);
    }
}
