#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffHolder.h"

// 按标识和规则删除只影响匹配实例，并取消各自的退出定时器。
TEST_CASE("BuffDestroy selects instances and cancels pending exits")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    BuffPool pool;
    for (uint64_t serial = 1; serial <= 3; ++serial)
    {
        BuffInstance instance;
        instance.m_serial = serial;
        instance.m_id = serial == 2 ? 25 : 1;
        pool.m_instances.push_back(instance);
        world.ecs().singleton<LogicTimers>().add(entity, "100"_fx, false, BuffExit{serial});
    }
    world.ecs().add(entity, pool);
    buff::BuffHolder(world, entity).destroyBuff(1);
    REQUIRE(world.ecs().get<BuffPool>(entity).m_instances.size() == 1);
    CHECK(world.ecs().get<BuffPool>(entity).m_instances.front().m_serial == 2);
    REQUIRE(world.ecs().singleton<LogicTimers>().entries.size() == 1);
    CHECK(std::get<BuffExit>(world.ecs().singleton<LogicTimers>().entries.front().payload).m_serial == 2);
    buff::BuffHolder(world, entity).destroyBuffByRule(conf::Config::instance().get<conf::BuffConfig>(25).ruleId);
    CHECK(world.ecs().get<BuffPool>(entity).m_instances.empty());
    CHECK(world.ecs().singleton<LogicTimers>().entries.empty());
    world.ecs().get<BuffPool>(entity).m_instances = pool.m_instances;
    buff::BuffHolder(world, entity).destroyBuff();
    CHECK(world.ecs().get<BuffPool>(entity).m_instances.empty());
}
