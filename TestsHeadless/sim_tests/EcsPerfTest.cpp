#include "doctest.h"
#include "EcsTestTypes.h"

#include <chrono>
#include <iostream>
#if defined(_WIN32)
#    include <process.h>
#endif

using namespace mugen;
using namespace ecs_test;

TEST_CASE("Ecs performance smoke 10k entities three components 1000 traversals")
{
    Manager ecs;
    for (int index = 0; index < 10000; ++index)
    {
        const Entity entity = ecs.create();
        ecs.add(entity, Position{index});
        ecs.add(entity, Velocity{1});
        ecs.add(entity, Weight{2});
    }
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 1000; ++iteration)
        ecs.each<Position, Velocity, Weight>(
            [](Entity, Position& p, Velocity& v, Weight& w) { p.value += v.value * w.value; });
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
    int64_t checksum = 0;
    ecs.each<Position>([&](Entity, Position& position) { checksum += position.value; });
    CHECK(checksum == 69995000);
    std::cout << "Ecs perf: 10000 entities x 3 components x 1000 each = " << elapsed.count()
              << " us (reference target 50000 us; informational), checksum=" << checksum << '\n';
}

#if defined(_WIN32) && !defined(NDEBUG)
TEST_CASE("Ecs missing get fires MG_ASSERT and probe executable really launches")
{
    const char* successArgs[] = {executablePath.c_str(), "--ecs-probe-ok", nullptr};
    REQUIRE(_spawnv(_P_WAIT, executablePath.c_str(), successArgs) == 0);
    const char* failureArgs[] = {executablePath.c_str(), "--ecs-missing-component-probe", nullptr};
    const intptr_t exitCode   = _spawnv(_P_WAIT, executablePath.c_str(), failureArgs);
    CHECK(exitCode == 3);
    std::cout << "Ecs MG_ASSERT child exit=" << exitCode << " (expected CRT abort=3)\n";
}
#endif
