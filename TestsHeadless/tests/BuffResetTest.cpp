#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffHolder.h"

// 全部移层和强制单层分支使用同一真实配置，刷新取消旧退出定时器。
TEST_CASE("BuffReset remove repeat all and forced single layer")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    REQUIRE(conf::Config::instance().get<conf::BuffConfig>(1).removeRepeatAll == 1);
    for (const bool force : {false, true})
    {
        World world;
        const auto entity = skill_test::actor(world);
        BuffPool pool;
        BuffInstance instance;
        instance.m_id = instance.m_initialId = 1;
        instance.m_serial = 1;
        instance.m_lifecycle.m_repeat = 3;
        pool.m_instances.push_back(instance);
        world.ecs().add(entity, pool);
        buff::BuffHolder(world, entity).removeRepeat(1, false, 1, force);
        const auto* result = buff::BuffHolder(world, entity).findInstance(1);
        REQUIRE(result != nullptr);
        CHECK(result->m_lifecycle.m_repeat == (force ? 2 : 0));
        CHECK(result->m_lifecycle.m_destroy == !force);
        CHECK(result->m_lifecycle.m_exit == !force);
        world.ecs().singleton<LogicTimers>().add(entity, "100"_fx, false, BuffExit{1});
        auto config = conf::Config::instance().get<conf::BuffConfig>(1);
        config.resetType = 1;
        buff::BuffHolder(world, entity).resetInstance(1, config);
        CHECK(world.ecs().singleton<LogicTimers>().entries.empty());
        CHECK_FALSE(buff::BuffHolder(world, entity).findInstance(1)->m_lifecycle.m_exit);
    }
}

// 使用真实配置的非即时规则验证实例重置，不绕过生产重置入口。
TEST_CASE("BuffReset all eight policies update instance counters and stacks")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    const auto& rows = conf::Config::instance().table<conf::BuffConfig>();
    const auto selected = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return row.executeType == 3 && row.repeatMax > 1;
    });
    REQUIRE(selected != rows.end());
    for (int32_t type = 0; type < 8; ++type)
    {
        World world;
        const auto entity = skill_test::actor(world);
        BuffPool pool;
        BuffInstance instance;
        instance.m_id = selected->id;
        instance.m_serial = 1;
        instance.m_lifecycle = buff::makeLifecycle(*selected);
        instance.m_lifecycle.m_times = 3;
        instance.m_lifecycle.m_interval = "2"_fx;
        pool.m_instances.push_back(instance);
        world.ecs().add(entity, pool);
        auto incoming = *selected;
        incoming.resetType = type;
        buff::BuffHolder(world, entity).resetInstance(1, incoming);
        const auto* result = buff::BuffHolder(world, entity).findInstance(1);
        REQUIRE(result != nullptr);
        CHECK(result->m_lifecycle.m_times == ((type == 1 || type == 2 || type == 6 || type == 7) ? 0 : 3));
        CHECK(result->m_lifecycle.m_interval == ((type == 1 || type == 3 || type == 5 || type == 7) ? Fixed{} : "2"_fx));
        CHECK(result->m_lifecycle.m_repeat == (type >= 4 ? 2 : 1));
        CHECK(result->m_id == selected->id);
    }
}

// 同标识叠层会再次执行旧配置，但死亡且非持续类型不得施加状态。
TEST_CASE("BuffReset stacking does not enter invincibility on a dead role")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    const auto config = conf::Config::instance().get<conf::BuffConfig>(201);
    REQUIRE(config.executeType == 1);
    REQUIRE(config.addType != 1);
    for (const bool dead : {false, true})
    {
        World world;
        const auto entity = skill_test::actor(world);
        if (dead) world.ecs().get<Vitals>(entity).hp = {};
        BuffPool pool;
        BuffInstance instance;
        instance.m_id = instance.m_initialId = config.id;
        instance.m_serial = 1;
        instance.m_lifecycle = buff::makeLifecycle(config);
        pool.m_instances.push_back(instance);
        world.ecs().add(entity, pool);
        auto incoming = config;
        incoming.resetType = 4;
        buff::BuffHolder(world, entity).resetInstance(1, incoming);
        CHECK(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Invincible) == !dead);
        CHECK(buff::BuffHolder(world, entity).findInstance(1)->m_appliedRefs == (dead ? 0 : 1));
    }
}
