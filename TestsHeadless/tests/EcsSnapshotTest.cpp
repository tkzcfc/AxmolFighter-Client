#include "doctest.h"
#include "EcsTestTypes.h"

#include <cstring>

using namespace mugen;
using namespace ecs_test;

TEST_CASE("Ecs snapshots round trip dense order free queue and pending destruction")
{
    Manager ecs;
    std::vector<Entity> entities;
    for (int index = 0; index < 6; ++index)
        entities.push_back(ecs.create());
    for (int index : {4, 2, 0, 3, 1, 5})
        ecs.add(entities[index], Position{index});
    ecs.add(entities[2], Payload{{4, 5, 6}, entities[3]});
    ecs.destroy(entities[1]);
    ecs.destroy(entities[0]);
    ecs.flushDestroyed();
    ecs.destroy(entities[5]);
    ecs.destroy(entities[3]);
    ecs.singleton<Clock>().tick     = 99;
    ecs.singleton<Queue>().entities = {entities[2], entities[3]};
    ByteBuffer first;
    ecs.serialize(first);
    Manager restored;
    REQUIRE(restored.deserialize(first));
    ByteBuffer second;
    restored.serialize(second);
    REQUIRE(first.len() == second.len());
    CHECK(std::memcmp(first.data(), second.data(), first.len()) == 0);
    CHECK(restored.hash() == ecs.hash());
    CHECK(restored.get<Payload>(entities[2]).target == entities[3]);
    const Manager& readOnly = restored;
    CHECK(readOnly.singleton<Clock>().tick == 99);
    CHECK(readOnly.singleton<Queue>().entities == ecs.singleton<Queue>().entities);
    restored.flushDestroyed();
    ecs.flushDestroyed();
    CHECK_FALSE(restored.alive(entities[5]));
    CHECK_FALSE(restored.alive(entities[3]));
    for (int index : {1, 0, 5, 3})
    {
        const Entity actual = restored.create();
        CHECK(actual == ecs.create());
        CHECK(actual.index() == entities[index].index());
    }
    CHECK(restored.hash() == ecs.hash());
}

TEST_CASE("Ecs clones deeply own component and singleton containers")
{
    Manager ecs;
    const Entity entity = ecs.create();
    ecs.add(entity, Payload{{1, 2}, entity});
    ecs.singleton<Queue>().entities = {entity};
    auto copy                       = ecs.clone();
    CHECK(copy.hash() == ecs.hash());
    copy.get<Payload>(entity).values[0] = 9;
    copy.singleton<Queue>().entities.clear();
    CHECK(ecs.get<Payload>(entity).values[0] == 1);
    CHECK(ecs.singleton<Queue>().entities.size() == 1);
    ecs.get<Payload>(entity).values.push_back(3);
    ecs.singleton<Clock>().tick = 8;
    CHECK(copy.get<Payload>(entity).values == std::vector<int32_t>{9, 2});
    CHECK(copy.singleton<Clock>().tick == 0);
    copy.destroy(entity);
    copy.flushDestroyed();
    CHECK(ecs.alive(entity));
}

TEST_CASE("Ecs hash is stable sensitive and exactly xxh64 of snapshot bytes")
{
    Manager a;
    Manager b;
    const Entity entity = a.create();
    CHECK(entity == b.create());
    a.add(entity, Position{42});
    b.add(entity, Position{42});
    CHECK(a.hash() == b.hash());
    ByteBuffer bytes;
    a.serialize(bytes);
    CHECK(a.hash() == xxh64(bytes.data(), bytes.len()));
    b.get<Position>(entity).value++;
    CHECK(a.hash() != b.hash());
    b.get<Position>(entity).value--;
    CHECK(a.hash() == b.hash());
    b.singleton<Clock>().tick++;
    CHECK(a.hash() != b.hash());
    b.singleton<Clock>().tick--;
    b.destroy(entity);
    CHECK(a.hash() != b.hash());
}

TEST_CASE("Ecs malformed snapshots fail transactionally and populated managers reject restore")
{
    Manager source;
    const Entity entity = source.create();
    source.add(entity, Payload{{1, 2, 3}, entity});
    ByteBuffer bytes;
    source.serialize(bytes);
    for (uint32_t size = 0; size < bytes.len(); ++size)
    {
        ByteBuffer truncated(bytes.data(), size);
        Manager target;
        const uint64_t before = target.hash();
        REQUIRE_FALSE(target.deserialize(truncated));
        REQUIRE(target.hash() == before);
    }
    Manager populated;
    populated.create();
    const uint64_t before = populated.hash();
    CHECK_FALSE(populated.deserialize(bytes));
    CHECK(populated.hash() == before);
    CHECK(bytes.getPosition() == 0);

    ByteBuffer badGeneration(bytes.data(), bytes.len());
    badGeneration.data()[4] = 0xff;
    Manager target;
    CHECK_FALSE(target.deserialize(badGeneration));
    ByteBuffer badSignature(bytes.data(), bytes.len());
    badSignature.data()[21] = 0x80;
    CHECK_FALSE(target.deserialize(badSignature));
}

TEST_CASE("Ecs empty registry manager snapshot round trips")
{
    ECSManager<> source;
    ByteBuffer first;
    source.serialize(first);
    ECSManager<> restored;
    REQUIRE(restored.deserialize(first));
    CHECK(restored.hash() == source.hash());
    CHECK(restored.create() == source.create());
    CHECK(restored.hash() == source.hash());
}
