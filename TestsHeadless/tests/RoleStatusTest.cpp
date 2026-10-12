#include "doctest.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"

TEST_CASE("Status attack refuses walk hit resets and death is terminal")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    world.init({});
    const auto entity = world.create(GroupKind::Role);
    world.ecs().add(entity, RoleStatus{});
    world.ecs().add(entity, RoleHitState{});
    CHECK(status::dealWithStatus(world, entity, EntityRoleStatus::Attack));
    CHECK_FALSE(status::dealWithStatus(world, entity, EntityRoleStatus::Walk));
    CHECK(status::dealWithStatus(world, entity, EntityRoleStatus::Hit, 42));
    auto& state = world.ecs().get<RoleStatus>(entity);
    CHECK(state.eEntityStatus == (EntityRoleStatus::Idle | EntityRoleStatus::Hit));
    CHECK(state.entityParams.hit_id == 42);
    CHECK(state.entityParams.hit_counts == 1);
    CHECK(status::isCanBreakStatus(state));
    CHECK(status::dealWithStatus(world, entity, EntityRoleStatus::Death, 133));
    CHECK_FALSE(status::dealWithStatus(world, entity, EntityRoleStatus::Attack));
    CHECK_FALSE(status::dealWithStatus(world, entity, EntityRoleStatus::Hit));
    CHECK_FALSE(status::dealWithStatus(world, entity, EntityRoleStatus::Idle));
    CHECK(state.eEntityStatus == EntityRoleStatus::Death);
    CHECK(state.entityParams.death_displacement_id == 133);
    ByteBuffer bytes;
    world.serialize(bytes);
    bytes.readFinish();
    World restored;
    REQUIRE(restored.deserialize(bytes));
    CHECK(restored.hash() == world.hash());
}

TEST_CASE("Status extra state references retain overlapping effects")
{
    mugen::sim::ExtraState state;
    state.add(mugen::ExtraState::Stun);
    state.add(mugen::ExtraState::Stun);
    state.remove(mugen::ExtraState::Stun);
    CHECK_FALSE(state.isCanControl());
    CHECK_FALSE(state.isCanMove());
    state.remove(mugen::ExtraState::Stun);
    CHECK(state.isCanControl());
    state.add(mugen::ExtraState::Durance);
    CHECK(state.isCanControl());
    CHECK_FALSE(state.isCanMove());
}
