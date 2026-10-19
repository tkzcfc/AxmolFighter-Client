#pragma once

#include "mugen/sim/World.h"

namespace gameui
{
class BattleDebugPanel
{
public:
    void render(mugen::sim::World& world, bool& paused, bool& singleStep, bool& collisionDebug);
};
}  // namespace gameui
