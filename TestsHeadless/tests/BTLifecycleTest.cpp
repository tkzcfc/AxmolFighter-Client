#include "doctest.h"  // IWYU pragma: keep
#include "BTTestTypes.h"

using namespace bt_test;

TEST_CASE("BT enter checks and enters each passing guard before stopping at first rejection")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 1, {0, 3}), action(0)}, {{0, 0}, {0, 1}, {0, 2}});
    f.ctx.gates[1] = false;
    StderrCapture capture;
    // When
    const bool entered = f.runner.enter(f.ctx);
    // Then
    CHECK_FALSE(entered);
    CHECK(f.instance.nodes[0].status == BTStatus::Failure);
    CHECK(f.instance.conditions == std::vector<BTStatus>{BTStatus::Running, BTStatus::Readied, BTStatus::Readied});
    CHECK(f.ctx.log == Log{"C0.check", "C0.enter", "C1.check"});
}

TEST_CASE("BT rejected branch unwinds only entered guards before trying the next child")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 2), composite(BTNodeType::Parallel, 1, {0, 3}), action(0), action(1)},
              {{0, 0}, {0, 1}, {0, 2}});
    f.ctx.gates[2] = false;
    // When
    const bool entered = f.runner.enter(f.ctx);
    // Then
    CHECK(entered);
    CHECK(f.ctx.log ==
          Log{"C0.check", "C0.enter", "C1.check", "C1.enter", "C2.check", "C1.exit", "C0.exit", "A1.enter"});
    CHECK(f.instance.nodes[0].currentIndex == 1);
}

TEST_CASE("BT update rechecks guards without reentering them")
{
    // Given
    Fixture f({composite(BTNodeType::Sequence, 1, {0, 2}), action(0)}, {{0, 0}, {0, 1}});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.log.clear();
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK(running);
    CHECK(f.ctx.log == Log{"C0.check", "C1.check", "A0.update:1"});
}

TEST_CASE("BT guard rejection succeeds and exits running children before parent guard cleanup")
{
    // Given
    Fixture f({composite(BTNodeType::Sequence, 1, {0, 2}), action(0)}, {{0, 0}, {0, 1}});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.gates[0] = false;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK_FALSE(running);
    CHECK(f.instance.nodes[0].status == BTStatus::Success);
    CHECK(f.instance.nodes[1].status == BTStatus::Readied);
    CHECK(f.instance.conditions[0] == BTStatus::Running);
    CHECK(f.ctx.log == Log{"C0.check", "A0.exit"});
}

TEST_CASE("BT exit resets parent before reverse condition exits for every composite")
{
    for (auto type : {BTNodeType::Selector, BTNodeType::Sequence, BTNodeType::Parallel})
    {
        // Given
        Fixture f({composite(type, 1, {0, 2}), action(0)}, {{0, 0}, {0, 1}});
        REQUIRE(f.runner.enter(f.ctx));
        f.ctx.log.clear();
        // When
        f.runner.exit(f.ctx);
        // Then
        CHECK(f.ctx.log == Log{"A0.exit", "C1.exit", "C0.exit"});
        REQUIRE(f.ctx.parentsAtExit.size() == 2);
        for (auto [status, cursor] : f.ctx.parentsAtExit)
        {
            CHECK(status == BTStatus::Readied);
            CHECK(cursor == -1);
        }
        CHECK(f.ctx.conditionsAtExit == std::vector<BTStatus>{BTStatus::Running, BTStatus::Running});
        CHECK(f.instance.conditions == std::vector<BTStatus>{BTStatus::Readied, BTStatus::Readied});
    }
}

TEST_CASE("BT leaf enter cannot finish the leaf and only update supplies its terminal status")
{
    // Given
    Fixture f({action(0)});
    f.ctx.results[0] = BTStatus::Success;
    // When
    const bool entered = f.runner.enter(f.ctx);
    // Then
    CHECK(entered);
    CHECK(f.instance.nodes[0].status == BTStatus::Running);
    CHECK(std::get<Counter>(f.instance.leaves[0]).ticks == 0);
    CHECK(f.ctx.log == Log{"A0.enter"});
}

TEST_CASE("BT stopped root logs once and never restarts on subsequent ticks")
{
    // Given
    Fixture f({action(0)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results[0] = BTStatus::Success;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    for (int tick = 0; tick < 4; ++tick)
        CHECK_FALSE(f.runner.update(f.ctx));
    // Then
    CHECK(f.ctx.log == Log{"A0.update:1"});
    CHECK(f.instance.nodes[0].status == BTStatus::Success);
    CHECK(f.instance.rootStopLogged);
    const auto text = capture.text();
    CHECK(std::count(text.begin(), text.end(), '\n') == 1);
    CHECK(text.starts_with("[E]"));
}

TEST_CASE("BT status replacement switches guarded branches using round robin without forced exits")
{
    // Given: earlier branch represents Hit; later branch represents Attack.
    auto pool = params();
    pool[0]   = Probe{0, 1, 2};
    pool[1]   = Probe{1, 3, 4};
    Fixture f({composite(BTNodeType::Selector, 2), composite(BTNodeType::Parallel, 1, {0, 1}), action(0),
               composite(BTNodeType::Selector, 1, {1, 1}), action(1)},
              {{0, 0}, {0, 1}}, std::move(pool));
    f.ctx.bits = 1 | 4;
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.bits = 1 | 2;
    f.ctx.log.clear();
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK(running);
    CHECK(f.instance.nodes[0].currentIndex == 0);
    CHECK(f.ctx.bits == (1 | 2));
    CHECK(f.ctx.log == Log{"C1.check", "A1.exit", "C1.exit", "C0.check", "C0.enter", "A0.enter"});
    REQUIRE(f.ctx.parentsAtExit.size() == 1);
    CHECK(f.ctx.parentsAtExit[0].first == BTStatus::Readied);
    CHECK(f.ctx.parentsAtExit[0].second == -1);
}
