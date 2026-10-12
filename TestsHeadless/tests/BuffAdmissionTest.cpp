#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffPool.h"

// 验证免疫与冷却边界，零冷却在下一次更新才删除。
TEST_CASE("BuffAdmission immunity cooldown and area checks")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    auto& pool = world.ecs().get<BuffPool>(entity);
    conf::BuffConfig config;
    config.id = 11;
    config.ruleId = 3;
    config.areaSetting = {-1};
    CHECK(buff::buffCheck(world, entity, config, 8));
    buff::setImmune(pool, 3, true);
    buff::setImmune(pool, 3, true);
    CHECK(pool.m_immunity.size() == 1);
    CHECK_FALSE(buff::buffCheck(world, entity, config, 8));
    buff::setImmune(pool, 3, false);
    buff::setImmune(pool, -1, true);
    CHECK_FALSE(buff::buffCheck(world, entity, config, 8));
    buff::setImmune(pool, -1, false);
    pool.m_cooldowns.push_back({11, "0.1"_fx});
    buff::updateCooldowns(pool, "0.1"_fx);
    CHECK_FALSE(buff::buffCheck(world, entity, config, 8));
    buff::updateCooldowns(pool, "0.1"_fx);
    CHECK(buff::buffCheck(world, entity, config, 8));
    config.areaSetting = {0, 8};
    CHECK_FALSE(buff::buffCheck(world, entity, config, 8));
    CHECK(buff::buffCheck(world, entity, config, 9));
    config.areaSetting = {1, 8};
    CHECK(buff::buffCheck(world, entity, config, 8));
    CHECK_FALSE(buff::buffCheck(world, entity, config, 9));
}

// 重复概率严格比较且只使用插入顺序中的第一个同类实例。
TEST_CASE("BuffAdmission probability repeat uses first matching stack")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    auto config = conf::Config::instance().get<conf::BuffConfig>(1);
    config.probability = "20"_fx;
    config.probabilityRepeat = 1;
    BuffPool pool;
    CHECK(buff::probabilityPass(pool, config, "19"_fx));
    CHECK_FALSE(buff::probabilityPass(pool, config, "20"_fx));
    BuffInstance instance;
    instance.m_id = 1;
    instance.m_serial = 1;
    instance.m_lifecycle.m_repeat = 3;
    pool.m_instances.push_back(instance);
    instance.m_serial = 2;
    instance.m_lifecycle.m_repeat = 5;
    pool.m_instances.push_back(instance);
    CHECK(buff::probabilityPass(pool, config, "59"_fx));
    CHECK_FALSE(buff::probabilityPass(pool, config, "60"_fx));
    config.probabilityRepeat = 0;
    CHECK_FALSE(buff::probabilityPass(pool, config, "59"_fx));
    config.probability = Fixed{};
    CHECK_FALSE(buff::probabilityPass(pool, config, Fixed{}));
}

// 减益免疫由池内规则身份决定，不要求该具体规则已移植。
TEST_CASE("BuffAdmission ImmuneDebuff blocks types three and four")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, BuffPool{});
    const auto& config = conf::Config::instance();
    int32_t immunityId = -1;
    for (const auto& row : config.table<conf::BuffConfig>())
    {
        const auto* rule = config.find<conf::BuffRuleConfig>(row.ruleId);
        if (rule && rule->className == "BuffImmuneDebuff") { immunityId = row.id; break; }
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
        CHECK(buff::buffCheck(world, entity, candidate, 0) == (type < 3));
    }
}
