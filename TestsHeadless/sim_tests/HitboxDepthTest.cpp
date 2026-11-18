#include "doctest.h"
#include "mugen/sim/World.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/anim/HitboxRules.h"
#include "mugen/core/math/DamageBox.h"

using namespace mugen;
using namespace mugen::sim;

namespace
{
WorldBox slab(int x, int depthCenter, int height, int width, int depth, int heightSize)
{
    return {{Fixed::fromInt(x), Fixed::fromInt(depthCenter) - Fixed::fromInt(depth) * Fixed::ratio(1, 2),
             Fixed::fromInt(height)},
            {Fixed::fromInt(width), Fixed::fromInt(depth), Fixed::fromInt(heightSize)}};
}

Entity addCollider(World& world, int32_t camp, int32_t hostile, WorldBox attack, WorldBox damage)
{
    const Entity entity = world.ecs().create();
    IdentityComponent identity;
    identity.camp        = camp;
    identity.hostileMask = hostile;
    identity.born.frame = 1;
    world.ecs().add(entity, identity);
    HitboxesComponent hitboxes;
    if (attack.size.x > Fixed{} || attack.size.y > Fixed{} || attack.size.z > Fixed{})
        hitboxes.attack.push_back(attack);
    if (damage.size.x > Fixed{} || damage.size.y > Fixed{} || damage.size.z > Fixed{})
        hitboxes.damage.push_back(damage);
    world.ecs().add(entity, hitboxes);
    return entity;
}
}  // namespace

TEST_CASE("placeCombatBox keeps height off the depth axis and centers an override")
{
    TransformComponent transform;
    transform.position = {Fixed::fromInt(10), Fixed::fromInt(80), Fixed::fromInt(500)};
    transform.scale    = Fixed::fromInt(2);
    DamageBox box;
    box.pos  = {3, 0, 4};
    box.size = {100, 10, 100};

    const WorldBox overridden = placeCombatBox(box, transform, Fixed::fromInt(2000));
    CHECK(overridden.pos.x == Fixed::fromInt(16));
    CHECK(overridden.size.x == Fixed::fromInt(200));
    CHECK(overridden.pos.z == Fixed::fromInt(88));
    CHECK(overridden.size.z == Fixed::fromInt(200));
    CHECK(overridden.pos.y == Fixed::fromInt(-500));
    CHECK(overridden.size.y == Fixed::fromInt(2000));

    const WorldBox fromFile = placeCombatBox(box, transform, {});
    CHECK(fromFile.pos.y == Fixed::fromInt(500));
    CHECK(fromFile.size.y == Fixed::fromInt(20));
}

TEST_CASE("collision hits a short box across a deep gap and misses a screen-aligned jumper")
{
    WorldSetup setup;
    setup.mode = WorldMode::Battle;
    World deep;
    deep.init(setup);
    const Entity slash = addCollider(deep, 1, 2, slab(0, 0, 0, 100, 2000, 100), {});
    addCollider(deep, 2, 1, {}, slab(0, 500, 0, 100, 2000, 100));
    runCollision(deep);
    REQUIRE(deep.ecs().get<ContactMemoryComponent>(slash).contacts.size() == 1);
    const ContactEvent& hit = deep.ecs().singleton<EventQueue<ContactEvent>>().items.front();
    CHECK(hit.point.y == Fixed::fromInt(50));
    CHECK(hit.point.z == Fixed::fromInt(250));

    World jumped;
    jumped.init(setup);
    const Entity air = addCollider(jumped, 1, 2, slab(0, 0, 200, 100, 40, 100), {});
    addCollider(jumped, 2, 1, {}, slab(0, 200, 0, 100, 40, 100));
    runCollision(jumped);
    CHECK(jumped.ecs().get<ContactMemoryComponent>(air).contacts.empty());
}
