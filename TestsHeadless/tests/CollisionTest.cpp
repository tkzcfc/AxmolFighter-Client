#include "SkillTestSupport.h"

using namespace skill_test;

namespace
{
struct CollisionFixture
{
    World world;
    uint64_t frame = 0;

    CollisionFixture() { world.init({}); }

    Entity makeCollider(FixedVec3 position, Fixed radius, WorldBox attack, WorldBox damage, uint64_t registered = 0)
    {
        const Entity entity = world.create(GroupKind::Effect);
        RoleIdentity identity;
        identity.camp = 2;
        identity.hostileCamp = 2;
        world.ecs().add(entity, identity);
        Transform transform;
        transform.position = position;
        world.ecs().add(entity, transform);
        ColliderState collider;
        collider.attack = {attack};
        collider.damage = {damage};
        collider.radius = radius;
        collider.registrationFrame = registered;
        world.ecs().add(entity, std::move(collider));
        return entity;
    }

    void tick()
    {
        FrameInput input;
        input.frame = ++frame;
        world.tick(input);
    }
};

WorldBox box(int64_t x, int64_t depth, int64_t screenY, int64_t width = 2, int64_t depthSize = 0, int64_t height = 2)
{
    return {{Fixed::fromInt(x), Fixed::fromInt(depth), Fixed::fromInt(screenY)},
            {Fixed::fromInt(width), Fixed::fromInt(depthSize), Fixed::fromInt(height)}};
}
}

TEST_CASE("Collision ignores box depth and observes screen-plane overlap")
{
    CollisionFixture flat;
    const auto flatSource = flat.makeCollider({}, "1"_fx, box(0, 0, 0), {});
    flat.makeCollider({}, "1"_fx, {}, box(1, 0, 1));
    flat.tick();
    REQUIRE(flat.world.ecs().get<CollisionState>(flatSource).contacts.size() == 1);

    CollisionFixture deep;
    const auto deepSource = deep.makeCollider({}, "1"_fx, box(0, 100000, 0, 2, 100000, 2), {});
    deep.makeCollider({}, "1"_fx, {}, box(1, -100000, 1, 2, 100000, 2));
    deep.tick();
    CHECK(deep.world.ecs().get<CollisionState>(deepSource).contacts.size() == 1);
}

TEST_CASE("Collision rejects depth radius separation and transitions contact state")
{
    CollisionFixture fixture;
    const auto source = fixture.makeCollider({}, "1"_fx, box(0, 0, 0), {});
    const auto target = fixture.makeCollider({{}, {}, "3"_fx}, "1"_fx, {}, box(1, 0, 1));
    fixture.tick();
    CHECK(fixture.world.ecs().get<CollisionState>(source).contacts.empty());

    fixture.world.ecs().get<Transform>(target).position.z = {};
    fixture.tick();
    REQUIRE(fixture.world.ecs().get<CollisionState>(source).contacts.size() == 1);
    const auto contact = fixture.world.ecs().get<CollisionState>(source).contacts.front();
    CHECK(contact.point.x == "1.5"_fx);
    CHECK(contact.point.y == "1.5"_fx);
    fixture.tick();
    CHECK(fixture.world.ecs().get<CollisionState>(source).contacts.size() == 1);

    fixture.world.ecs().get<ColliderState>(target).damage.front().pos.x = "10"_fx;
    fixture.tick();
    CHECK(fixture.world.ecs().get<CollisionState>(source).contacts.empty());
}

TEST_CASE("Collision drops registration frame and cleans destroyed contacts")
{
    CollisionFixture fixture;
    const auto source = fixture.makeCollider({}, "1"_fx, box(0, 0, 0), {}, 1);
    const auto target = fixture.makeCollider({}, "1"_fx, {}, box(1, 0, 1), 1);
    fixture.tick();
    CHECK(fixture.world.ecs().get<CollisionState>(source).contacts.empty());

    fixture.world.ecs().get<ColliderState>(source).registrationFrame = 0;
    fixture.world.ecs().get<ColliderState>(target).registrationFrame = 0;
    fixture.tick();
    REQUIRE(fixture.world.ecs().get<CollisionState>(source).contacts.size() == 1);
    fixture.world.destroy(target);
    fixture.tick();
    CHECK(fixture.world.ecs().get<CollisionState>(source).contacts.empty());
}

TEST_CASE("Collision uses motion mirroring world boxes")
{
    CollisionFixture fixture;
    Transform facing;
    facing.position.x = "10"_fx;
    facing.towardX = -1;
    DamageBox local;
    local.pos = {3, 999999, 2};
    local.size = {2, 999999, 2};
    const auto mirrored = role::transformBox(local, facing);
    CHECK(mirrored.pos.x == "5"_fx);
    CHECK(mirrored.pos.z == "2"_fx);
}
