#include <doctest.h>

#include "mugen/sim/input/ControlManagerCore.h"

using namespace mugen::sim;

TEST_CASE("ControlManager combo emits joystick event")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    for (ControlKey key : {ControlKey::Down, ControlKey::Left, ControlKey::Up, ControlKey::Right, ControlKey::Down})
    {
        control.setKey(key, true);
        control.advance(commands);
        control.setKey(key, false);
        control.advance(commands);
    }
    control.setKey(ControlKey::AttackA, true);
    control.advance(commands);
    REQUIRE(!commands.empty());
    CHECK(commands.back().controlEvent == mugen::ControlEvent::CTR_JOY1);
}

TEST_CASE("ControlManager repeats held basic attack on fixed interval")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    control.setKey(ControlKey::AttackA, true);
    for (uint32_t frame = 0; frame < 13; ++frame)
        control.advance(commands);
    uint32_t count = 0;
    for (const auto& command : commands)
        count += command.controlEvent == mugen::ControlEvent::CTR_ATKA;
    CHECK(count == 3);
}

TEST_CASE("ControlManager diagonal Up Right emits quadrant 1 at 45 degrees")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    control.setKey(ControlKey::Up, true);
    control.setKey(ControlKey::Right, true);
    control.advance(commands);
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].controlEvent == mugen::ControlEvent::CTR_RUN);
    CHECK(commands[0].quadrant == ControlQuadrant::UpRight);
    CHECK(commands[0].angle == 45000);
}

TEST_CASE("ControlManager Right then Up reemits the same quadrant at 45 degrees")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    control.setKey(ControlKey::Right, true);
    control.advance(commands);
    control.setKey(ControlKey::Up, true);
    control.advance(commands);
    REQUIRE(commands.size() == 2);
    CHECK(commands[1].controlEvent == mugen::ControlEvent::CTR_RUN);
    CHECK(commands[1].quadrant == ControlQuadrant::UpRight);
    CHECK(commands[1].angle == 45000);
}

TEST_CASE("ControlManager Left then Right while both held runs right")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    control.setKey(ControlKey::Left, true);
    control.advance(commands);
    control.setKey(ControlKey::Right, true);
    control.advance(commands);
    REQUIRE(commands.size() == 2);
    CHECK(commands[1].controlEvent == mugen::ControlEvent::CTR_RUN);
    CHECK(commands[1].quadrant == ControlQuadrant::UpRight);
    CHECK(commands[1].angle == 0);
}

TEST_CASE("ControlManager releasing the later opposing key keeps the remaining direction")
{
    ControlManagerCore control;
    std::vector<PlayerCommand> commands;
    control.setKey(ControlKey::Left, true);
    control.advance(commands);
    control.setKey(ControlKey::Right, true);
    control.advance(commands);
    commands.clear();
    control.setKey(ControlKey::Right, false);
    control.advance(commands);
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].controlEvent == mugen::ControlEvent::CTR_RUN);
    CHECK(commands[0].quadrant == ControlQuadrant::DownLeft);
    CHECK(commands[0].angle == 0);
}
