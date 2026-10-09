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
