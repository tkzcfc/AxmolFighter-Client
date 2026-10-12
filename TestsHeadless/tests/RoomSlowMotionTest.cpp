#include <doctest.h>
#include "mugen/sim/World.h"

using namespace mugen;
using namespace mugen::sim;

TEST_CASE("Room KO curve preserves sequential phase boundaries")
{
    room::KoSlowMotion curve;
    curve.start();
    curve.update("0.25"_fx);
    CHECK(curve.m_scale == "0.1"_fx + "0.5"_fx * "0.9"_fx);
    curve.update("0.25"_fx);
    CHECK(curve.m_scale == "0.1"_fx);
    curve.update("1"_fx);
    CHECK(curve.m_scale == "0.1"_fx);
    curve.update("0.5"_fx);
    CHECK(curve.m_scale == "0.1"_fx + "0.5"_fx * "0.9"_fx);
    curve.update("0.6"_fx);
    CHECK_FALSE(curve.m_active);
    CHECK(curve.m_scale == "1"_fx);
    curve.start();
    curve.update("0.75"_fx);
    CHECK(curve.m_fallSeconds == "0.75"_fx);
    CHECK(curve.m_holdSeconds == Fixed{});
    CHECK(curve.m_scale == "0.1"_fx);
}

TEST_CASE("Room KO World frames and snapshot continuation use unscaled curve time")
{
    World world;
    world.init({});
    world.ecs().singleton<SlowMotion>().m_ko.start();
    for (uint64_t frame = 1; frame <= 45; ++frame)
    {
        FrameInput input;
        input.frame = frame;
        world.tick(input);
        const auto& slow = world.ecs().singleton<SlowMotion>();
        if (frame == 1)
            CHECK(slow.scale == "0.1"_fx + ("1"_fx - kLogicDtSec / "0.5"_fx) * "0.9"_fx);
        if (frame == 16 || frame == 45)
            CHECK(slow.scale == "0.1"_fx);
        CHECK(world.ecs().singleton<WorldClock>().dtSec == kLogicDtSec * slow.scale);
    }
    ByteBuffer buffer;
    world.serialize(buffer);
    World restored;
    REQUIRE(restored.deserialize(buffer));
    World clone = world.clone();
    for (uint64_t frame = 46; frame <= 79; ++frame)
    {
        FrameInput input;
        input.frame = frame;
        world.tick(input);
        restored.tick(input);
        clone.tick(input);
        CHECK(world.hash() == restored.hash());
        CHECK(world.hash() == clone.hash());
        if (frame == 46)
            CHECK(world.ecs().singleton<SlowMotion>().scale == "0.1"_fx + kLogicDtSec * "0.9"_fx);
    }
    CHECK_FALSE(world.ecs().singleton<SlowMotion>().m_ko.m_active);
    CHECK(world.ecs().singleton<WorldClock>().dtSec == kLogicDtSec);
}
