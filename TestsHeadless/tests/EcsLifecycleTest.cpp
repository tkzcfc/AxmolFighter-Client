#include "doctest.h"
#include "EcsTestTypes.h"
#include "mugen/core/ecs/System.h"

using namespace mugen;
using namespace ecs_test;

TEST_CASE("Ecs stale handles cannot read or remove recycled components")
{
    Manager ecs;
    const Entity first = ecs.create();
    CHECK(first.index() == 0);
    CHECK(first.generation() == 1);
    CHECK_FALSE(ecs.alive({}));
    CHECK(ecs.tryGet<Position>({}) == nullptr);
    CHECK(ecs.tryGet<Position>(first) == nullptr);
    ecs.add(first, Position{7});
    ecs.destroy(first);
    CHECK(ecs.alive(first));
    CHECK(ecs.get<Position>(first).value == 7);
    ecs.flushDestroyed();
    CHECK_FALSE(ecs.alive(first));
    CHECK(ecs.tryGet<Position>(first) == nullptr);
    const Entity reused = ecs.create();
    CHECK(reused.index() == first.index());
    CHECK(reused.generation() == first.generation() + 1);
    CHECK_FALSE(ecs.has<Position>(reused));
    ecs.add(reused, Position{9});
    ecs.remove<Position>(first);
    ecs.destroy(first);
    ecs.flushDestroyed();
    CHECK(ecs.get<Position>(reused).value == 9);
    CHECK(ecs.tryGet<Position>(first) == nullptr);
    const Manager& readOnly = ecs;
    CHECK(readOnly.tryGet<Position>(reused)->value == 9);
    CHECK(readOnly.get<Position>(reused).value == 9);
}

TEST_CASE("Ecs free slots recycle FIFO including reverse creation destruction")
{
    Manager ecs;
    const Entity a = ecs.create();
    const Entity b = ecs.create();
    const Entity c = ecs.create();
    ecs.destroy(a);
    ecs.destroy(b);
    ecs.destroy(a);
    ecs.flushDestroyed();
    const Entity d = ecs.create();
    const Entity e = ecs.create();
    CHECK(d.index() == a.index());
    CHECK(e.index() == b.index());
    ecs.destroy(e);
    ecs.destroy(c);
    ecs.destroy(d);
    ecs.flushDestroyed();
    CHECK(ecs.create().index() == e.index());
    CHECK(ecs.create().index() == c.index());
    CHECK(ecs.create().index() == d.index());
}

TEST_CASE("Ecs traversal fixes upper bound and defers stable removal until flush")
{
    Manager ecs;
    const Entity a = ecs.create();
    const Entity b = ecs.create();
    const Entity c = ecs.create();
    for (Entity entity : {a, b, c})
        ecs.add(entity, Position{entity.index()});
    std::vector<Entity> visited;
    Entity spawned;
    ecs.each<Position>([&](Entity entity, Position&) {
        visited.push_back(entity);
        if (entity == a)
        {
            ecs.destroy(a);
            ecs.destroy(b);
            spawned = ecs.create();
            ecs.add(spawned, Position{10});
        }
    });
    CHECK(visited == std::vector<Entity>{a, b, c});
    CHECK(ecs.alive(a));
    CHECK(ecs.alive(b));
    CHECK(ecs.get<Position>(b).value == 1);
    ecs.flushDestroyed();
    CHECK_FALSE(ecs.alive(a));
    CHECK_FALSE(ecs.alive(b));
    const Entity reused = ecs.create();
    ecs.add(reused, Position{11});
    visited.clear();
    ecs.each<Position>([&](Entity entity, Position&) { visited.push_back(entity); });
    CHECK(visited == std::vector<Entity>{c, spawned, reused});
}

TEST_CASE("Ecs sparse pool swap removal preserves typed joins and creation order")
{
    Manager ecs;
    const Entity a = ecs.create();
    const Entity b = ecs.create();
    const Entity c = ecs.create();
    ecs.add(c, Position{30});
    ecs.add(a, Position{10});
    ecs.add(b, Position{20});
    ecs.add(a, Velocity{2});
    ecs.add(c, Velocity{3});
    ecs.remove<Position>(a);
    CHECK(ecs.get<Position>(b).value == 20);
    CHECK(ecs.get<Position>(c).value == 30);
    ecs.remove<Position>(a);
    ecs.add(a, Position{11});
    std::vector<Entity> visited;
    ecs.each<Position, Velocity>([&](Entity entity, Position& position, Velocity& velocity) {
        visited.push_back(entity);
        position.value += velocity.value;
    });
    CHECK(visited == std::vector<Entity>{a, c});
    CHECK(ecs.get<Position>(a).value == 13);
    CHECK(ecs.get<Position>(c).value == 33);
    CHECK(ecs.get<Position>(b).value == 20);
}

TEST_CASE("Ecs nested traversals each capture their own upper bound")
{
    Manager ecs;
    const Entity a = ecs.create();
    std::vector<Entity> outer;
    std::vector<Entity> inner;
    Entity b;
    ecs.each<>([&](Entity entity) {
        outer.push_back(entity);
        b = ecs.create();
        ecs.each<>([&](Entity nested) { inner.push_back(nested); });
    });
    CHECK(outer == std::vector<Entity>{a});
    CHECK(inner == std::vector<Entity>{a, b});
}

TEST_CASE("Ecs exhausted generations retire without resurrecting stale handles")
{
    ECSManager<> ecs;
    const Entity first = ecs.create();
    Entity entity      = first;
    for (uint32_t generation = 1; generation <= Entity::kMaxGeneration; ++generation)
    {
        REQUIRE(entity.generation() == generation);
        ecs.destroy(entity);
        ecs.flushDestroyed();
        entity = ecs.create();
    }
    CHECK(entity.index() == 1);
    CHECK(entity.generation() == 1);
    CHECK_FALSE(ecs.alive(first));
    static_assert(std::is_abstract_v<System>);
}
