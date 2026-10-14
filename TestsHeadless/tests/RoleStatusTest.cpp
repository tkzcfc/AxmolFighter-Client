#include "doctest.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/Role.h"

TEST_CASE("Status attack refuses walk hit resets and death is terminal")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    world.init({});
    const auto entity = world.create(GroupKind::Role);
    world.ecs().add(entity, RoleStatus{});
    world.ecs().add(entity, RoleHitState{});
    CHECK(Role(world, entity).changeStatus(EntityRoleStatus::Attack));
    CHECK_FALSE(Role(world, entity).changeStatus(EntityRoleStatus::Walk));
    CHECK(Role(world, entity).changeStatus(EntityRoleStatus::Hit, 42));
    auto& state = world.ecs().get<RoleStatus>(entity);
    CHECK(state.eEntityStatus == (EntityRoleStatus::Idle | EntityRoleStatus::Hit));
    CHECK(state.entityParams.hit_id == 42);
    CHECK(state.entityParams.hit_counts == 1);
    CHECK(state.isCanBreak());
    CHECK(Role(world, entity).changeStatus(EntityRoleStatus::Death, 133));
    CHECK_FALSE(Role(world, entity).changeStatus(EntityRoleStatus::Attack));
    CHECK_FALSE(Role(world, entity).changeStatus(EntityRoleStatus::Hit));
    CHECK_FALSE(Role(world, entity).changeStatus(EntityRoleStatus::Idle));
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
