#include "doctest.h"
#include "mugen/sim/buff/BuffLifecycle.h"

using namespace mugen;
using namespace mugen::sim::buff;

// 验证周期检查先于秒制累加以及每帧一次的规则。
TEST_CASE("BuffLifecycle interval branches retain check before accumulation")
{
    conf::BuffConfig config;
    config.interval = "0.1"_fx;
    for (int32_t times : {-1, 0, 2})
    {
        config.times = times;
        auto state = makeLifecycle(config);
        execute(state, 2);
        CHECK(advance(state, "0.1"_fx) == LifecycleAction::None);
        const auto expected = times == -1 ? LifecycleAction::Update :
            times == 0 ? LifecycleAction::RemoveRepeat : LifecycleAction::Enter;
        CHECK(advance(state, "0.1"_fx) == expected);
        CHECK(state.m_interval == Fixed{});
    }
    config.interval = Fixed{};
    for (int32_t times : {-2, -1, 0, 2})
    {
        config.times = times;
        auto state = makeLifecycle(config);
        execute(state, 2);
        const auto expected = times == -1 ? LifecycleAction::Update :
            times == 2 ? LifecycleAction::Enter : LifecycleAction::None;
        CHECK(advance(state, "0.033"_fx) == expected);
    }
    config.times = 2;
    config.interval = "-1"_fx;
    auto state = makeLifecycle(config);
    execute(state, 2);
    CHECK(advance(state, "1"_fx) == LifecycleAction::None);
}

// 八种重置只改变规定的计数，不擅自修改层数。
TEST_CASE("BuffLifecycle eight reset counter policies")
{
    for (int32_t type = 0; type < 8; ++type)
    {
        Lifecycle state;
        state.m_times = 3;
        state.m_interval = "2"_fx;
        state.m_repeat = 4;
        resetCounters(state, type);
        CHECK(state.m_times == ((type == 1 || type == 2 || type == 6 || type == 7) ? 0 : 3));
        CHECK(state.m_interval == ((type == 1 || type == 3 || type == 5 || type == 7) ? Fixed{} : "2"_fx));
        CHECK(state.m_repeat == 4);
    }
}

// 执行模式和内部冷却独立于规则回调的具体效果。
TEST_CASE("BuffLifecycle execution modes and inner cooldown")
{
    conf::BuffConfig config;
    config.times = 2;
    auto state = makeLifecycle(config);
    CHECK(execute(state, 1) == LifecycleAction::Enter);
    CHECK(state.m_times == 1);
    CHECK_FALSE(finishEnter(state));
    CHECK(advance(state, "0.1"_fx) == LifecycleAction::Enter);
    CHECK(finishEnter(state));
    CHECK(state.m_times == 0);
    CHECK(execute(state, 3) == LifecycleAction::None);
    CHECK_FALSE(state.m_active);
    CHECK(advance(state, "0.1"_fx) == LifecycleAction::None);
    CHECK(execute(state, 2) == LifecycleAction::None);
    CHECK(state.m_active);
    config.times = -1;
    config.innerCd = "0.2"_fx;
    state = makeLifecycle(config);
    execute(state, 2);
    CHECK(advance(state, "0.1"_fx) == LifecycleAction::Update);
    CHECK(advance(state, "0.1"_fx) == LifecycleAction::None);
    CHECK(advance(state, "0.1"_fx) == LifecycleAction::Update);
    state.m_exit = true;
    CHECK(advance(state, "1"_fx) == LifecycleAction::None);
}
