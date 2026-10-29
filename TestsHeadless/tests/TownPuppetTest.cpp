#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/spawn/RoleSpawn.h"

TEST_CASE("Town puppet updates synchronize position, facing, and motion")
{
    using namespace mugen;
    using namespace mugen::sim;

    REQUIRE((mugen::test::ensureContentVfs() && conf::Config::instance().load("/mugen/config/config.bin")));
    World world;
    world.init({1, WorldMode::Town, 41, {}});
    const Entity puppet = RoleSpawner(world).spawnPuppet(77, 261, {}, 1);

    FrameInput input;
    input.frame = 1;
    input.puppetUpdates.push_back({puppet, {"123.5"_fx, "2"_fx, "-45.25"_fx}, -1, EntityRoleStatus::Walk, "running"});
    world.tick(input);

    const auto& transform = world.ecs().get<Transform>(puppet);
    CHECK(transform.position.x == "123.5"_fx);
    CHECK(transform.position.y == "2"_fx);
    CHECK(transform.position.z == "-45.25"_fx);
    CHECK(transform.towardX == -1);
    CHECK(world.ecs().get<RoleStatus>(puppet).eEntityStatus == EntityRoleStatus::Walk);
    CHECK(world.ecs().get<MotionState>(puppet).motionName == "running");
    CHECK(world.ecs().has<Puppet>(puppet));
    CHECK_FALSE(world.ecs().has<RoleBrain>(puppet));
}

TEST_CASE("Town ignores a puppet update for an entity that does not exist")
{
    using namespace mugen;
    using namespace mugen::sim;

    World world;
    world.init({1, WorldMode::Town, 41, {}});
    FrameInput input;
    input.frame = 1;
    input.puppetUpdates.push_back({Entity{1}, {}, 1, EntityRoleStatus::Idle, "stand"});
    CHECK_NOTHROW(world.tick(input));
}
