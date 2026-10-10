#include "doctest.h"
#include "mugen/sim/room/RoomEntities.h"
#include "../support/ScenarioHarness.h"

using namespace mugen;
using namespace mugen::sim;

TEST_CASE("Room 20002 portals keep an empty motion file and a radius damage box")
{
    test::ScenarioHarness::loadAssets();
    auto world = test::ScenarioHarness::roomWorld(20002);
    for (int i = 0; i < 30; ++i)
    {
        FrameInput input;
        input.frame = world.ecs().singleton<WorldClock>().frame + 1;
        world.tick(input);
    }
    int32_t portals = 0;
    world.ecs().each<PortalState>([&](Entity entity, const PortalState&) {
        ++portals;
        CHECK(world.ecs().get<MotionState>(entity).motionFile.empty());
        CHECK_FALSE(world.ecs().get<ColliderState>(entity).damage.empty());
    });
    REQUIRE(portals > 0);
}
