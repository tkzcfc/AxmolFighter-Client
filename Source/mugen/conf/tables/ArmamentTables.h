#pragma once

#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>

namespace mugen::conf
{

// Slim armament (武装 / GodWeapon artifact) row for client UI / local cheat state.
// Full heiyue item_artifact_* migration can expand this later.
struct ArmamentConfig
{
    int32_t comboSkill = 0;
    int32_t descId     = 0;
    std::string icon;
    std::string iconBig;
    std::string iconList;
    int32_t id         = 0;
    int32_t nameId     = 0;
    int32_t occupation = 0;
    int32_t quality    = 0;
    int32_t skin       = 0;

    MG_REFLECT(ArmamentConfig, comboSkill, descId, icon, iconBig, iconList, id, nameId, occupation, quality, skin)
};

}  // namespace mugen::conf
