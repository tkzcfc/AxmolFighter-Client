#pragma once

#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>

namespace mugen::conf
{

// Slim pet (宠物 / item_spirit) row for client UI / local cheat state.
struct PetConfig
{
    int32_t descId = 0;
    std::string icon;
    std::string iconMax;
    int32_t id       = 0;
    int32_t levelMax = 100;
    int32_t nameId   = 0;
    // Linked entity_pet / battle unit id when present.
    int32_t petId   = 0;
    int32_t quality = 0;
    int32_t rare    = 0;
    int32_t starMax = 25;

    MG_REFLECT(PetConfig, descId, icon, iconMax, id, levelMax, nameId, petId, quality, rare, starMax)
};

}  // namespace mugen::conf
