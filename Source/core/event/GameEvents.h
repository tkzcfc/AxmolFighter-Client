#pragma once

#include <cstdint>

namespace game
{

// App-level game events (fashion / loadout / companions). Payload is empty;
// listeners re-read GameSession / LocalState.
enum class GameEvent : int32_t
{
    FashionWornChanged = 1,
    ArmamentAppearanceChanged,
    PetFollowChanged,
    PartnerActiveChanged,
};

}  // namespace game
