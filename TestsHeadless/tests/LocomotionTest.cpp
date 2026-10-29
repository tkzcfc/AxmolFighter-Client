#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/conf/BattleConst.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cmath>

using namespace mugen;
using namespace mugen::sim;
namespace
{
Entity setup(World& world, bool moving = false)
{
    REQUIRE((mugen::test::ensureContentVfs() && conf::Config::instance().load("/mugen/config/config.bin")));
    world.init({});
    RoleSpawnParams params;
    params.roleId = 261;
    if (moving)
    {
        const auto& rows = conf::Config::instance().table<conf::RoleConfig>();
        const auto row   = std::find_if(rows.begin(), rows.end(), [](const auto& value) {
            return value.roleType == 1 && value.velocity > Fixed{} && !value.aiId.empty();
        });
        REQUIRE(row != rows.end());
        params.roleId = row->id;
    }
    params.level      = 100;
    params.playerSlot = 0;
    return RoleSpawner(world).spawnRole(params);
}
void advance(World& world, Entity entity, ControlEvent event)
{
    world.ecs().get<ControlEventQueue>(entity).commands.push_back({event, ControlQuadrant::UpRight, 0, 0});
    world.tick({world.ecs().singleton<WorldClock>().frame + 1, {}, {}, {}});
}
double number(Fixed value)
{
    return static_cast<double>(value.raw) / Fixed::kOne;
}
}  // namespace
TEST_CASE("Locomotion run right thirty frames stops at map edge and release returns Idle")
{
    World world;
    const Entity entity = setup(world);
    const Fixed speed   = conf::Config::instance().get<conf::RoleConfig>(261).velocity * conf::kRunRate;
    for (int i = 0; i < 30; ++i)
        advance(world, entity, ControlEvent::CTR_RUN);
    CHECK(number(world.ecs().get<Transform>(entity).position.x) ==
          doctest::Approx(number(speed * kLogicDtMs * Fixed::fromInt(29))).epsilon(0.00001));
    auto& map                                     = world.ecs().singleton<MapRegion>();
    map.moveRanges                                = {{"-100"_fx, "-100"_fx, "300"_fx, "200"_fx}};
    world.ecs().get<Transform>(entity).position.x = "200"_fx - world.ecs().get<ColliderState>(entity).radius;
    advance(world, entity, ControlEvent::CTR_RUN);
    CHECK(world.ecs().get<Transform>(entity).position.x == "200"_fx - world.ecs().get<ColliderState>(entity).radius);
    advance(world, entity, ControlEvent::CTR_IDLE);
    CHECK(world.ecs().get<RoleBrain>(entity).m_branch == role::brain::RoleBranch::Idle);
    CHECK(world.ecs().get<MotionState>(entity).motionName == "stand");
    CHECK(world.ecs().get<RoleHitState>(entity).forceVelocity.x == Fixed{});
    auto clone = world.clone();
    advance(world, entity, ControlEvent::CTR_RUN);
    advance(clone, entity, ControlEvent::CTR_RUN);
    CHECK(world.hash() == clone.hash());
}
TEST_CASE("Locomotion applies ControlManager arrow commands to player movement")
{
    World world;
    const Entity entity = setup(world, true);
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    ControlKey key           = ControlKey::Right;
    ControlQuadrant quadrant = ControlQuadrant::UpRight;
    int32_t angle            = 0;
    int32_t dx = 1, dz = 0;
    SUBCASE("Right moves positive x") {}
    SUBCASE("Down moves negative z")
    {
        key      = ControlKey::Down;
        quadrant = ControlQuadrant::DownRight;
        angle    = 90000;
        dx       = 0;
        dz       = -1;
    }
    SUBCASE("Left moves negative x")
    {
        key      = ControlKey::Left;
        quadrant = ControlQuadrant::DownLeft;
        dx       = -1;
    }
    SUBCASE("Up moves positive z")
    {
        key      = ControlKey::Up;
        quadrant = ControlQuadrant::UpLeft;
        angle    = 90000;
        dx       = 0;
        dz       = 1;
    }
    world.ecs().singleton<MapRegion>().moveRanges = {{"-10000"_fx, "-10000"_fx, "20000"_fx, "20000"_fx}};
    control.setKey(key, true);
    control.advance(commands);
    REQUIRE(commands.size() == 1);
    CHECK(commands.front().controlEvent == ControlEvent::CTR_RUN);
    CHECK(commands.front().quadrant == quadrant);
    CHECK(commands.front().angle == angle);

    const auto before = world.ecs().get<Transform>(entity).position;
    FrameInput input;
    input.frame = world.ecs().singleton<WorldClock>().frame + 1;
    input.players.push_back({0, std::move(commands)});
    world.tick(input);
    CHECK(world.ecs().singleton<PlayerSlots>().entities.contains(0));
    CHECK(world.ecs().get<RoleControl>(entity).m_last.quadrant == quadrant);
    CHECK(world.ecs().get<RoleStatus>(entity).has(EntityRoleStatus::Run));
    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        input.frame = world.ecs().singleton<WorldClock>().frame + 1;
        world.tick(input);
    }
    const auto after = world.ecs().get<Transform>(entity).position;
    if (dx == 0)
        CHECK(after.x == before.x);
    else
        CHECK((after.x - before.x) * Fixed::fromInt(dx) > Fixed{});
    if (dz == 0)
        CHECK(after.z == before.z);
    else
        CHECK((after.z - before.z) * Fixed::fromInt(dz) > Fixed{});
}
TEST_CASE("Displacement zero VelocityScale preserves falling height")
{
    World world;
    const Entity entity                           = setup(world);
    world.ecs().get<Transform>(entity).position.y = "100"_fx;
    world.ecs().get<sim::ExtraState>(entity).add(mugen::ExtraState::Durance);
    world.ecs().get<Displacement>(entity).velocityScale = {};
    displacement::DisplacementDriver(world, entity).update();
    CHECK(world.ecs().get<Transform>(entity).position.y < "100"_fx);
    CHECK(world.ecs().get<RoleHitState>(entity).forceVelocity.y < Fixed{});
    for (int i = 0; i < 60; ++i)
        displacement::DisplacementDriver(world, entity).update();
    CHECK(world.ecs().get<Transform>(entity).position.y == Fixed{});
}
TEST_CASE("Action animation names and playback mapping")
{
    CHECK(std::string(behavior::ActionNames::animationName(behavior::ActionType::Run)) == "walk");
    CHECK(std::string(behavior::ActionNames::animationName(behavior::ActionType::Walk)) == "running");
    World world;
    const auto entity = setup(world);
    advance(world, entity, ControlEvent::CTR_WALK);
    CHECK(world.ecs().get<MotionState>(entity).actionScale == conf::kWalkScaleTime);
}
TEST_CASE("Displacement golden three ids match positions and velocities per frame")
{
    World world;
    const auto entity = setup(world);
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path() / "golden/displacement_golden.csv");
    REQUIRE(file.is_open());
    std::string line;
    int previous = -1, count = 0;
    double maxError = 0;
    while (std::getline(file, line))
    {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream input(line);
        int id, frame;
        REQUIRE(static_cast<bool>(input >> id >> frame));
        if (id != previous)
        {
            displacement::DisplacementDriver(world, entity).reset();
            world.ecs().get<Transform>(entity).position = {};
            REQUIRE(displacement::DisplacementDriver(world, entity).set(id));
            previous = id;
        }
        displacement::DisplacementDriver(world, entity).update();
        const auto p = world.ecs().get<Transform>(entity).position;
        const auto v = world.ecs().get<RoleHitState>(entity).forceVelocity;
        for (Fixed actual : {p.x, p.y, p.z, v.x, v.y, v.z})
        {
            double expected;
            REQUIRE(static_cast<bool>(input >> expected));
            const double error = std::abs(number(actual) - expected);
            maxError           = std::max(maxError, error);
            INFO("id=", id, " frame=", frame, " maxError=", maxError);
            CHECK(error < 0.001);
        }
        ++count;
    }
    CHECK(count == 270);
}
