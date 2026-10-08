#pragma once

#include "mugen/core/ecs/ECSManager.h"

namespace ecs_test
{
inline std::string executablePath;

struct Position
{
    int64_t value = 0;
    MG_REFLECT(Position, value)
};
struct Velocity
{
    int64_t value = 1;
    MG_REFLECT(Velocity, value)
};
struct Weight
{
    int64_t value = 2;
    MG_REFLECT(Weight, value)
};
struct Payload
{
    std::vector<int32_t> values;
    mugen::Entity target;
    MG_REFLECT(Payload, values, target)
};
struct Clock
{
    uint64_t tick = 0;
    MG_REFLECT(Clock, tick)
};
struct Queue
{
    std::vector<mugen::Entity> entities;
    MG_REFLECT(Queue, entities)
};

// Test-only registries. Production lists belong to sim/Components.h and sim/Singletons.h in Todo 12.
#define MG_COMPONENT_LIST(X) X(Position) X(Velocity) X(Weight) X(Payload)
#define MG_SINGLETON_LIST(X) X(Clock) X(Queue)
#define MG_ECS_TEST_TUPLE(T) \
    , std::tuple<T> {}
using Components = decltype(std::tuple_cat(std::tuple<> {} MG_COMPONENT_LIST(MG_ECS_TEST_TUPLE)));
using Singletons = decltype(std::tuple_cat(std::tuple<> {} MG_SINGLETON_LIST(MG_ECS_TEST_TUPLE)));
using Manager    = mugen::ECSManager<Components, Singletons>;
#undef MG_ECS_TEST_TUPLE
#undef MG_COMPONENT_LIST
#undef MG_SINGLETON_LIST

static_assert(Manager::componentId<Position> == 0);
static_assert(Manager::componentId<Velocity> == 1);
static_assert(Manager::componentId<Weight> == 2);
static_assert(Manager::componentId<Payload> == 3);
static_assert(Manager::singletonId<Clock> == 0);
static_assert(Manager::singletonId<Queue> == 1);
}  // namespace ecs_test
