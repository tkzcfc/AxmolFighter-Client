#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace gameui
{

// Per-armament runtime state (local / cheat; not synced to server yet).
struct WeaponInstanceState
{
    int32_t configId   = 0;
    int32_t level      = 1;
    int32_t star       = 0;
    int32_t grade      = 0;  // 进阶
    int32_t skinId     = 0;  // appearance skin (0 = default)
    int32_t skillId    = 0;  // configured skill (0 = table comboSkill)
    bool unlocked      = true;
};

// Open params for FeatureEntry::Weapon (optional).
struct WeaponOpenParams
{
    int32_t focusArmamentId = 0;
};

}  // namespace gameui
