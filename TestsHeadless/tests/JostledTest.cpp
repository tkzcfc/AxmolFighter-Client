#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/role/brain/RoleBrainRunner.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/role/Role.h"

using namespace mugen;
using namespace mugen::sim;

TEST_CASE("Portal Finding consumes ordered path points before returning Idle")
{
    test::ScenarioHarness::loadAssets();
    World world;
    world.init({});
    RoleSpawnParams params;
    const auto& rows = conf::Config::instance().table<conf::RoleConfig>();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const auto& value) {
        return value.roleType == 1 && value.velocity > Fixed{} && !value.aiId.empty();
    });
    REQUIRE(row != rows.end());
    params.roleId = row->id;
    const auto entity = spawnRole(world, params);
    auto& ecs = world.ecs();
    const auto step = row->velocity * Fixed::ratio(1000, 60);
    const FixedVec3 first{step / "2"_fx, {}, {}};
    const FixedVec3 last{step, {}, {}};
    ecs.get<RoleStatus>(entity).entityParams.m_path = {first, last};
    REQUIRE(Role(world, entity).changeStatus(EntityRoleStatus::Finding));
    for (int i = 0; i < 8 && !ecs.get<RoleStatus>(entity).entityParams.m_path.empty(); ++i)
        role::brain::update(ecs.get<RoleBrain>(entity), world, entity, false);
    CHECK(ecs.get<RoleStatus>(entity).entityParams.m_path.empty());
    CHECK(ecs.get<Transform>(entity).position.x == last.x);
    role::brain::update(ecs.get<RoleBrain>(entity), world, entity, false);
    CHECK_FALSE(ecs.get<RoleStatus>(entity).isFinding());
    CHECK(ecs.get<RoleHitState>(entity).forceVelocity.x == Fixed{});
}

TEST_CASE("Portal Jostled registered leaf completes on brake and resets displacement")
{
    test::ScenarioHarness::loadAssets();
    for (const bool horizontal : {false, true})
    {
        int32_t id = -1;
        for (const auto& row : conf::Config::instance().table<conf::DisplacementConfig>())
            if (!row.velocity.empty() && (row.velocity[0] != Fixed{}) == horizontal)
            {
                id = row.id;
                break;
            }
        REQUIRE(id != -1);
        World world;
        world.init({});
        const auto entity = world.create(GroupKind::Role);
        auto& ecs = world.ecs();
        RoleStatus status;
        status.eEntityStatus = EntityRoleStatus::Jostled;
        status.entityParams.displacement_id = id;
        ecs.add(entity, status);
        ecs.add(entity, MotionState{});
        Transform transform;
        transform.vectorX = "1"_fx;
        ecs.add(entity, transform);
        ecs.add(entity, RoleIdentity{});
        ecs.add(entity, RoleHitState{});
        ecs.add(entity, Displacement{});
        ecs.add(entity, RoleBrain{});
        ecs.add(entity, RoleControl{});
        ecs.add(entity, sim::ExtraState{});
        ecs.add(entity, ControlEventQueue{});
        role::brain::update(ecs.get<RoleBrain>(entity), world, entity, false);
        CHECK(ecs.get<Transform>(entity).towardX == (horizontal ? -1 : 1));
        CHECK(ecs.get<Transform>(entity).vectorX == "1"_fx);
        CHECK(ecs.get<MotionState>(entity).motionName == "running");
        REQUIRE(ecs.get<Displacement>(entity).listenerNode >= 0);
        CHECK_FALSE(role::brain::onDisplacementEvent(world, entity, DisplacementEvent::Floor));
        CHECK(ecs.get<Transform>(entity).vectorX == "1"_fx);
        PlayerCommand command;
        command.controlEvent = ControlEvent::CTR_RUN;
        ecs.get<ControlEventQueue>(entity).commands.push_back(command);
        role::brain::consumeControl(world, entity);
        CHECK(ecs.get<RoleStatus>(entity).eEntityStatus == EntityRoleStatus::Jostled);
        CHECK(ecs.get<ControlEventQueue>(entity).commands.empty());
        CHECK_FALSE(role::brain::onDisplacementEvent(world, entity, DisplacementEvent::Brake));
        const auto expected = horizontal ? "-1"_fx : "1"_fx;
        CHECK(ecs.get<Transform>(entity).vectorX == expected);
        CHECK_FALSE(role::brain::onDisplacementEvent(world, entity, DisplacementEvent::Brake));
        CHECK(ecs.get<Transform>(entity).vectorX == expected);
        role::brain::update(ecs.get<RoleBrain>(entity), world, entity, false);
        CHECK(ecs.get<Displacement>(entity).listenerNode == -1);
        CHECK(ecs.get<RoleHitState>(entity).forceVelocity.x == Fixed{});
    }
}
