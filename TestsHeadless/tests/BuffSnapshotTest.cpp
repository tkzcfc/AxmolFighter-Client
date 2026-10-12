#include "doctest.h"
#include "mugen/sim/World.h"

// 克隆与归档必须保留实例插入顺序、冷却、免疫和规则引用计数。
TEST_CASE("BuffPool component survives clone and snapshot without shared state")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    world.init({});
    const auto entity = world.create(GroupKind::Role);
    BuffPool pool;
    BuffInstance first;
    first.m_serial = 1;
    first.m_id = 201;
    first.m_context.m_target = entity;
    first.m_context.m_boundSkill = 920000;
    first.m_lifecycle.m_interval = "0.2"_fx;
    first.m_lifecycle.m_repeat = 3;
    pool.m_instances.push_back(first);
    first.m_serial = 2;
    first.m_id = 154;
    pool.m_instances.push_back(first);
    pool.m_cooldowns.push_back({201, "0.5"_fx});
    pool.m_immunity = {-1, 7};
    pool.m_ruleCounters[3] = 2;
    pool.m_nextSerial = 3;
    world.ecs().add(entity, pool);

    auto clone = world.clone();
    REQUIRE(clone.hash() == world.hash());
    clone.ecs().get<BuffPool>(entity).m_instances.front().m_lifecycle.m_repeat = 9;
    CHECK(world.ecs().get<BuffPool>(entity).m_instances.front().m_lifecycle.m_repeat == 3);
    CHECK(clone.hash() != world.hash());

    ByteBuffer bytes;
    world.serialize(bytes);
    World restored;
    REQUIRE(restored.deserialize(bytes));
    REQUIRE(restored.hash() == world.hash());
    const auto& loaded = restored.ecs().get<BuffPool>(entity);
    REQUIRE(loaded.m_instances.size() == 2);
    CHECK(loaded.m_instances[0].m_id == 201);
    CHECK(loaded.m_instances[1].m_id == 154);
    CHECK(loaded.m_instances[0].m_context.m_target == entity);
    CHECK(loaded.m_instances[0].m_context.m_boundSkill == 920000);
    REQUIRE(loaded.m_cooldowns.size() == 1);
    CHECK(loaded.m_cooldowns[0].m_seconds == "0.5"_fx);
    CHECK(loaded.m_immunity == std::vector<int32_t>{-1, 7});
    CHECK(loaded.m_ruleCounters.at(3) == 2);
    CHECK(loaded.m_nextSerial == 3);
}
