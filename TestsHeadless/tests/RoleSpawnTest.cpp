#include "doctest.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/role/RoleUpdate.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>
#include "mugen/avatar/data/AvatarAssetCache.h"

namespace ecs_test
{
extern std::string executablePath;
}

namespace
{
void loadConfig()
{
    REQUIRE(mugen::conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"));
}
}  // namespace
TEST_CASE("Attribute golden compares three role curves at two levels")
{
    loadConfig();
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path() / "golden/attribute_golden.csv");
    REQUIRE(file.is_open());
    std::string line;
    int rows = 0;
    while (std::getline(file, line))
    {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream input(line);
        int id, level;
        REQUIRE(static_cast<bool>(input >> id >> level));
        const auto actual =
            mugen::sim::loadAttributes(mugen::conf::Config::instance().get<mugen::conf::RoleConfig>(id), level);
        for (const auto value : actual.basic)
        {
            double expected;
            REQUIRE(static_cast<bool>(input >> expected));
            const double observed = static_cast<double>(value.raw) / mugen::Fixed::kOne;
            INFO("role=", id, " level=", level, " expected=", expected, " actual=", observed);
            CHECK(std::abs(observed - expected) < 0.0001);
            if (expected == std::floor(expected))
                CHECK(observed == expected);
        }
        ++rows;
    }
    CHECK(rows == 6);
}
TEST_CASE("Spawn freezes component stage while MP regeneration continues")
{
    using namespace mugen;
    using namespace mugen::sim;
    loadConfig();
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId     = 261;
    params.level      = 100;
    params.isAi       = true;
    params.playerSlot = 0;
    const auto entity = spawnRole(world, params);
    auto& vitals      = world.ecs().get<Vitals>(entity);
    CHECK(vitals.hp == vitals.maxHp);
    CHECK(vitals.mp == Fixed::fromInt(100));
    CHECK(vitals.ep == Fixed{});
    vitals.mp                                    = {};
    world.ecs().get<RoleTimers>(entity).freezeMs = Fixed::fromInt(50);
    world.tick({1, {}, {}, {}});
    CHECK(vitals.mp > Fixed{});
    CHECK(world.ecs().get<RoleTimers>(entity).freezeMs > Fixed{});
    CHECK(world.ecs().singleton<PlayerSlots>().entities.at(0) == entity);
    world.tick({2, {}, {}, {}});
    CHECK(world.ecs().get<RoleTimers>(entity).freezeMs == Fixed{});
    CHECK(world.clone().hash() == world.hash());
}
TEST_CASE("Collider mirrors scaled box about role position")
{
    using namespace mugen;
    sim::Transform t;
    t.position = {Fixed::fromInt(100), Fixed::fromInt(10), Fixed::fromInt(20)};
    t.scale    = Fixed::fromInt(2);
    DamageBox box{{3, 4, 5}, {7, 8, 9}};
    const auto right = sim::role::transformBox(box, t);
    t.towardX        = -1;
    const auto left  = sim::role::transformBox(box, t);
    CHECK(right.pos.x == Fixed::fromInt(106));
    CHECK(left.pos.x == Fixed::fromInt(80));
    CHECK(left.size.x == right.size.x);
    CHECK(left.pos.z == Fixed::fromInt(40));
}

TEST_CASE("Collider real motion advances after freeze expires and restores identically")
{
    using namespace mugen;
    using namespace mugen::sim;
    loadConfig();
    auto* cache = AvatarAssetCache::getInstance();
    cache->addSearchPath("res_zhcn");
    REQUIRE(cache->load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/avatar.bin"));
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId                                = 261;
    params.level                                 = 100;
    const auto entity                            = spawnRole(world, params);
    auto& motion                                 = world.ecs().get<MotionState>(entity);
    motion.motionFile                            = "mugen/motion/hero/rw_newzhujue_lilimu.motion";
    motion.motionName                            = "101";
    motion.playing                               = true;
    motion.actionScale                           = Fixed::fromInt(2);
    world.ecs().get<RoleTimers>(entity).staticMs = Fixed::fromInt(50);
    world.tick({1, {}, {}, {}});
    CHECK(motion.timeMs == Fixed{});
    world.tick({2, {}, {}, {}});
    CHECK(motion.timeMs == kLogicDtMs * Fixed::fromInt(2));
    CHECK_FALSE(world.ecs().get<ColliderState>(entity).damage.empty());
    ByteBuffer bytes;
    world.serialize(bytes);
    World restored;
    REQUIRE(restored.deserialize(bytes));
    world.tick({3, {}, {}, {}});
    restored.tick({3, {}, {}, {}});
    CHECK(restored.hash() == world.hash());
}

TEST_CASE("Spawn MapRegion loads real layer and clamps half unit boundaries")
{
    using namespace mugen;
    sim::MapRegion region;
    region.load(std::string(MUGEN_CONTENT_DIR) + "/mugen/map/stage9_room5_2.layer");
    REQUIRE_FALSE(region.moveRanges.empty());
    region.moveRanges = {{"7.5"_fx, "-13.5"_fx, "5"_fx, "7"_fx}};
    CHECK(region.getLeftRegion({"0"_fx, "-10"_fx}) == "7.5"_fx);
    CHECK(region.getRightRegion({"20"_fx, "-10"_fx}) == "12.5"_fx);
    CHECK(region.getBottomRegion({"10"_fx, "-20"_fx}) == "-13.5"_fx);
    CHECK(region.getTopRegion({"10"_fx, "0"_fx}) == "-6.5"_fx);
    CHECK(region.getLeftRegion({"10"_fx, "-10"_fx}) == "10"_fx);
}
#if !defined(NDEBUG)
TEST_CASE("Spawn invalid role asserts in Debug")
{
    const std::string executable = "\"" + ecs_test::executablePath + "\"";
    REQUIRE(std::system((executable + " --ecs-probe-ok").c_str()) == 0);
    CHECK(std::system((executable + " --spawn-invalid-role-probe").c_str()) == 3);
}
#endif
