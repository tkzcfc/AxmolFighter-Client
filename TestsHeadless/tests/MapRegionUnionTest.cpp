#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim_legacy/motion/MapRegion.h"
#include "mugen/sim_legacy/motion/Displacement.h"
#include "mugen/sim_legacy/spawn/RoleSpawn.h"
#include "mugen/conf/ReflectedConfig.h"

using namespace mugen;
using namespace mugen::sim_legacy;

namespace
{
// 城镇 41 的三块可移动范围（r1 包裹 r2/r3 的横轴区间）。
void loadTown41Ranges(MapRegion& region)
{
    region.moveRanges = {
        {"132"_fx, "14"_fx, "2629"_fx, "198"_fx},   // r1 x[132,2761] z[14,212]
        {"355"_fx, "195"_fx, "836"_fx, "55"_fx},    // r2 x[355,1191] z[195,250]
        {"1883"_fx, "195"_fx, "875"_fx, "44"_fx},   // r3 x[1883,2758] z[195,239]
    };
}
}  // namespace

TEST_CASE("MapRegion union semantics across nested town ranges")
{
    MapRegion region;
    loadTown41Ranges(region);

    // 向右走：点在 r1 内，r2 的右边界（1191）不是墙，右边界必须是 r1 的 2761。
    CHECK(region.getRightRegion({"850"_fx, "200"_fx}) == "850"_fx);     // r1 内，无墙
    CHECK(region.getRightRegion({"1195"_fx, "200"_fx}) == "1195"_fx);   // 已越过 r2 右边界，仍在 r1 内
    CHECK(region.getRightRegion({"2761"_fx, "200"_fx}) == "2761"_fx);   // 恰好落在 r1 右边界
    CHECK(region.getRightRegion({"2765"_fx, "200"_fx}) == "2761"_fx);   // 越过 r1 右边界，钳回 2761

    // 向左走：点从 850 向左，r3 的左边界（1883）不是墙，不得被拉回 >850。
    CHECK(region.getLeftRegion({"830"_fx, "200"_fx}) == "830"_fx);      // r1 内，无墙
    CHECK(region.getLeftRegion({"100"_fx, "200"_fx}) == "132"_fx);      // 越过 r1 左边界，钳回 132

    // 在 x=2142 向上走：点属于 r3，顶边界必须是 r3 的 239，而不是 r1 的 212。
    CHECK(region.getTopRegion({"2142"_fx, "232"_fx}) == "232"_fx);      // r3 内，无墙
    CHECK(region.getTopRegion({"2142"_fx, "240"_fx}) == "239"_fx);      // 越过 r3 顶边界，钳回 239
}

TEST_CASE("Town walker passes nested range edges and never teleports")
{
    REQUIRE((mugen::test::ensureContentVfs() && conf::Config::instance().load("/mugen/config/config.bin")));
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId     = 261;
    params.level      = 100;
    params.playerSlot = 0;
    const Entity entity = RoleSpawner(world).spawnRole(params);

    loadTown41Ranges(world.ecs().singleton<MapRegion>());
    const Fixed radius = world.ecs().get<ColliderState>(entity).radius;
    auto& t            = world.ecs().get<Transform>(entity);
    auto& hit          = world.ecs().get<RoleHitState>(entity);

    // 从 (850,200) 向右走：应越过 r2 右边界 1171，并最终停在 r1 右边界减半径处。
    t.position          = {"850"_fx, {}, "200"_fx};
    hit.forceVelocity.x = "2"_fx;
    for (int i = 0; i < 200; ++i)
        displacement::DisplacementDriver(world, entity).update();
    CHECK(t.position.x > "1171"_fx);
    CHECK(t.position.x == "2761"_fx - radius);

    // 从 (850,200) 向左走：只能向左移动，不得被瞬移回 >850。
    displacement::DisplacementDriver(world, entity).reset();
    t.position          = {"850"_fx, {}, "200"_fx};
    hit.forceVelocity.x = "-2"_fx;
    for (int i = 0; i < 8; ++i)
        displacement::DisplacementDriver(world, entity).update();
    CHECK(t.position.x < "850"_fx);
    CHECK(t.position.x > "132"_fx);

    // 在 x=2142 向上走：应能越过 r1 顶边界 212，抵达 r3 顶边界减半径处。
    displacement::DisplacementDriver(world, entity).reset();
    t.position          = {"2142"_fx, {}, "200"_fx};
    hit.forceVelocity.z = "2"_fx;
    for (int i = 0; i < 30; ++i)
        displacement::DisplacementDriver(world, entity).update();
    CHECK(t.position.z == "239"_fx - radius);

    // 在 r1 顶边附近（x 位于 r2/r3 之间的缺口）向右上斜走：
    // 不得被 r2 的右边界 1191 拉回，只能沿 r1 顶边滑动。
    displacement::DisplacementDriver(world, entity).reset();
    t.position          = {"1500"_fx, {}, "190"_fx};
    hit.forceVelocity.x = "2"_fx;
    hit.forceVelocity.z = "1"_fx;
    for (int i = 0; i < 4; ++i)
    {
        const Fixed lastX = t.position.x;
        displacement::DisplacementDriver(world, entity).update();
        CHECK(t.position.x > lastX);
    }
    CHECK(t.position.x > "1500"_fx);
    CHECK(t.position.z <= "212"_fx - radius);
}
