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

// depthCenter 是纵深中心，height 是高度最小角。尺寸为零的轴不会相交。
WorldBox box(int64_t x, int64_t depthCenter, int64_t height, int64_t width = 2, int64_t depth = 2, int64_t heightSize = 2)
{
    const Fixed depthStart = Fixed::fromInt(depthCenter) - Fixed::fromInt(depth) * Fixed::ratio(1, 2);
    return {{Fixed::fromInt(x), depthStart, Fixed::fromInt(height)},
            {Fixed::fromInt(width), Fixed::fromInt(depth), Fixed::fromInt(heightSize)}};
}
}

TEST_CASE("Collision hits a short box across a deep gap and misses a screen-aligned jumper")
{
    CollisionFixture deep;
    const auto slash = deep.makeCollider({}, "1"_fx, box(0, 0, 0, 100, 2000, 100), {});
    deep.makeCollider({{}, {}, "500"_fx}, "1"_fx, {}, box(0, 500, 0, 100, 2000, 100));
    deep.tick();
    REQUIRE(deep.world.ecs().get<CollisionState>(slash).contacts.size() == 1);
    const auto hit = deep.world.ecs().get<CollisionState>(slash).contacts.front();
    CHECK(hit.point.x == "50"_fx);
    CHECK(hit.point.y == "50"_fx);
    CHECK(hit.point.z == "250"_fx);

    CollisionFixture jumped;
    const auto air = jumped.makeCollider({}, "1"_fx, box(0, 0, 200, 100, 40, 100), {});
    jumped.makeCollider({{}, {}, "200"_fx}, "1"_fx, {}, box(0, 200, 0, 100, 40, 100));
    jumped.tick();
    CHECK(jumped.world.ecs().get<CollisionState>(air).contacts.empty());
}

TEST_CASE("Collision separates on depth and transitions contact state")
{
    CollisionFixture fixture;
    const auto source = fixture.makeCollider({}, "1"_fx, box(0, 0, 0), {});
    const auto target = fixture.makeCollider({}, "1"_fx, {}, box(1, 20, 1));
    fixture.tick();
    CHECK(fixture.world.ecs().get<CollisionState>(source).contacts.empty());

    fixture.world.ecs().get<ColliderState>(target).damage.front() = box(1, 0, 1);
    fixture.tick();
    REQUIRE(fixture.world.ecs().get<CollisionState>(source).contacts.size() == 1);
    const auto contact = fixture.world.ecs().get<CollisionState>(source).contacts.front();
    CHECK(contact.point.x == "1.5"_fx);
    CHECK(contact.point.y == "1.5"_fx);
    CHECK(contact.point.z == Fixed{});
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
    local.pos = {3, 0, 2};
    local.size = {2, 10, 2};
    const auto mirrored = role::RoleActor::transformBox(local, facing);
    CHECK(mirrored.pos.x == "5"_fx);
    CHECK(mirrored.pos.y == Fixed{});
    CHECK(mirrored.pos.z == "2"_fx);
    CHECK(mirrored.size.y == "10"_fx);

    const auto overridden = role::RoleActor::transformBox(local, facing, "40"_fx);
    CHECK(overridden.pos.y == "-20"_fx);
    CHECK(overridden.size.y == "40"_fx);
    CHECK(overridden.pos.z == "2"_fx);
}
