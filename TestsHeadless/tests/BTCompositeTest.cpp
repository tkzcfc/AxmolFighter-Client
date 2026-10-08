#include "doctest.h"  // IWYU pragma: keep
#include "BTTestTypes.h"

using namespace bt_test;

TEST_CASE("BT Selector chooses first enterable child and never preempts it")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 2), composite(BTNodeType::Parallel, 1, {0, 1}), action(0), action(1)},
              {{0, 0}});
    f.ctx.gates[0] = false;
    REQUIRE(f.runner.enter(f.ctx));
    REQUIRE(f.instance.nodes[0].currentIndex == 1);
    f.ctx.gates[0] = true;
    f.ctx.log.clear();
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK(running);
    CHECK(f.ctx.log == Log{"A1.update:1"});
    CHECK(f.instance.nodes[0].currentIndex == 1);
}

TEST_CASE("BT Selector rotates forward and wraps without updating newly entered children")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 3), action(0), action(1), action(2)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results.fill(BTStatus::Success);
    f.ctx.log.clear();
    // When
    for (int tick = 0; tick < 3; ++tick)
        REQUIRE(f.runner.update(f.ctx));
    // Then
    CHECK(f.ctx.log == Log{"A0.update:1", "A0.exit", "A1.enter", "A1.update:1", "A1.exit", "A2.enter", "A2.update:1",
                           "A2.exit", "A0.enter"});
    CHECK(f.instance.nodes[0].currentIndex == 0);
}

TEST_CASE("BT Selector succeeds when only its just completed child could enter")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 2), action(0), composite(BTNodeType::Parallel, 1, {0, 1}), action(1)},
              {{0, 0}});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.gates[0]   = false;
    f.ctx.results[0] = BTStatus::Success;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK_FALSE(running);
    CHECK(f.instance.nodes[0].status == BTStatus::Success);
    CHECK(f.ctx.log == Log{"A0.update:1", "A0.exit", "C0.check"});
}

TEST_CASE("BT single child Selector completes without reentering the same child")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 1), action(0)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results[0] = BTStatus::Success;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK_FALSE(running);
    CHECK(f.ctx.log == Log{"A0.update:1", "A0.exit"});
}

TEST_CASE("BT Selector ignores child Failure and warns only on the transition in debug")
{
    // Given
    Fixture f({composite(BTNodeType::Selector, 2), action(0), action(1)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results[0] = BTStatus::Failure;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    for (int tick = 0; tick < 3; ++tick)
        REQUIRE(f.runner.update(f.ctx));
    // Then
    CHECK(f.instance.nodes[0].status == BTStatus::Running);
    CHECK(f.instance.nodes[1].status == BTStatus::Failure);
    CHECK(f.ctx.log == Log{"A0.update:1"});
#ifndef NDEBUG
    const auto text = capture.text();
    CHECK(std::count(text.begin(), text.end(), '\n') == 1);
    CHECK(text.starts_with("[W]"));
#else
    CHECK(capture.text().empty());
#endif
}

TEST_CASE("BT Sequence treats child Failure as Running without advancing")
{
    // Given
    Fixture f({composite(BTNodeType::Sequence, 2), action(0), action(1)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results[0] = BTStatus::Failure;
    f.ctx.log.clear();
    // When
    for (int tick = 0; tick < 3; ++tick)
        REQUIRE(f.runner.update(f.ctx));
    // Then
    CHECK(f.instance.nodes[0].currentIndex == 0);
    CHECK(f.instance.nodes[1].status == BTStatus::Failure);
    CHECK(f.ctx.log == Log{"A0.update:1"});
}

TEST_CASE("BT Sequence advances on success and finishes successfully when next enter fails")
{
    // Given
    Fixture f({composite(BTNodeType::Sequence, 3), action(0), composite(BTNodeType::Parallel, 1, {0, 1}), action(1),
               action(2)},
              {{0, 0}});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results[0] = BTStatus::Success;
    f.ctx.gates[0]   = false;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK_FALSE(running);
    CHECK(f.instance.nodes[0].status == BTStatus::Success);
    CHECK(f.ctx.log == Log{"A0.update:1", "A0.exit", "C0.check"});
}

TEST_CASE("BT Sequence executes each child on a separate tick in order")
{
    // Given
    Fixture f({composite(BTNodeType::Sequence, 2), action(0), action(1)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results.fill(BTStatus::Success);
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    const bool first  = f.runner.update(f.ctx);
    const bool second = f.runner.update(f.ctx);
    // Then
    CHECK(first);
    CHECK_FALSE(second);
    CHECK(f.ctx.log == Log{"A0.update:1", "A0.exit", "A1.enter", "A1.update:1", "A1.exit"});
}

TEST_CASE("BT Parallel enters all children and removes either terminal result in stable order")
{
    // Given
    Fixture f({composite(BTNodeType::Parallel, 3), action(0), action(1), action(2)});
    REQUIRE(f.runner.enter(f.ctx));
    REQUIRE(f.ctx.log == Log{"A0.enter", "A1.enter", "A2.enter"});
    f.ctx.results = {BTStatus::Success, BTStatus::Running, BTStatus::Failure};
    f.ctx.log.clear();
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK(running);
    CHECK(f.instance.nodes[0].running == std::vector<uint16_t>{1});
    CHECK(f.ctx.log == Log{"A0.update:1", "A0.exit", "A1.update:1", "A2.update:1", "A2.exit"});
}

TEST_CASE("BT Parallel succeeds rather than failing when every running child fails")
{
    // Given
    Fixture f({composite(BTNodeType::Parallel, 2), action(0), action(1)});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.results.fill(BTStatus::Failure);
    StderrCapture capture;
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK_FALSE(running);
    CHECK(f.instance.nodes[0].status == BTStatus::Success);
    CHECK(f.instance.nodes[0].running.empty());
}

TEST_CASE("BT Parallel guard failure exits its complete running set in child order")
{
    // Given
    Fixture f({composite(BTNodeType::Parallel, 2, {0, 1}), action(0), action(1)}, {{0, 0}});
    REQUIRE(f.runner.enter(f.ctx));
    f.ctx.gates[0] = false;
    f.ctx.log.clear();
    StderrCapture capture;
    // When
    const bool running = f.runner.update(f.ctx);
    // Then
    CHECK_FALSE(running);
    CHECK(f.instance.nodes[0].status == BTStatus::Success);
    CHECK(f.instance.nodes[0].running.empty());
    CHECK(f.ctx.log == Log{"C0.check", "A0.exit", "A1.exit"});
}
