#include "doctest/doctest.h"

#include "ui/widgets/common/ProgressSmoother.h"

using gameui::ProgressSmoother;

TEST_SUITE("ProgressSmoother")
{
    TEST_CASE("display value never decreases, even if target drops")
    {
        ProgressSmoother smoother;
        float last = 0.0f;
        smoother.update(0.1f, 0.5f, false);
        CHECK_GE(smoother.getDisplay(), last);
        last = smoother.getDisplay();

        // 真实进度回退（不应该发生，但做个保护），显示值依然只增不减
        smoother.update(0.1f, 0.2f, false);
        CHECK_GE(smoother.getDisplay(), last);
    }

    TEST_CASE("catches up towards the target but never jumps instantly")
    {
        ProgressSmoother smoother;
        smoother.update(1.0f / 60.0f, 0.2f, false);
        // 一帧内不应该直接跳到目标值
        CHECK_LT(smoother.getDisplay(), 0.2f);
        CHECK_GT(smoother.getDisplay(), 0.0f);
    }

    TEST_CASE("after catching up, keeps creeping past a stalled target towards the ceiling")
    {
        ProgressSmoother smoother;
        for (int i = 0; i < 300; ++i)
            smoother.update(1.0f / 60.0f, 0.3f, false);
        // 追上之后会继续往上爬一点，不会卡死在 target 本身
        CHECK_GT(smoother.getDisplay(), 0.3f);
        CHECK_LT(smoother.getDisplay(), 0.95f);
    }

    TEST_CASE("creeping stays below the ceiling while not finished")
    {
        ProgressSmoother smoother;
        // 真实进度一直停在 10%，多跑几秒钟
        for (int i = 0; i < 600; ++i)
            smoother.update(1.0f / 60.0f, 0.1f, false);
        CHECK_LT(smoother.getDisplay(), 0.95f);
        CHECK_FALSE(smoother.isDone());
    }

    TEST_CASE("finish ramps up to 100% within a bounded time")
    {
        ProgressSmoother smoother;
        smoother.update(1.0f / 60.0f, 0.4f, false);
        for (int i = 0; i < 120 && !smoother.isDone(); ++i)
            smoother.update(1.0f / 60.0f, 1.0f, true);
        CHECK(smoother.isDone());
        CHECK_EQ(smoother.getDisplay(), doctest::Approx(1.0f));
    }

    TEST_CASE("once finished, stays finished even if target/finished flips back")
    {
        ProgressSmoother smoother;
        smoother.update(1.0f, 1.0f, true);
        CHECK(smoother.isDone());
        smoother.update(1.0f / 60.0f, 0.0f, false);
        CHECK(smoother.isDone());
    }
}
