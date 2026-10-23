#pragma once

#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>

namespace mugen::conf
{

// Slim partner (伙伴 / item_partner) row for client UI / local cheat state.
struct PartnerConfig
{
    int32_t descId = 0;
    std::string icon;
    int32_t id       = 0;
    int32_t levelMax = 100;
    int32_t nameId   = 0;
    int32_t quality  = 0;
    int32_t rare     = 0;
    // item_partner.spine_id + heiyue UiPartner preview anim (stand).
    std::string spineAnim;
    int32_t spineId = 0;
    int32_t starMax = 20;

    MG_REFLECT(PartnerConfig,
               descId,
               icon,
               id,
               levelMax,
               nameId,
               quality,
               rare,
               spineAnim,
               spineId,
               starMax)
};

}  // namespace mugen::conf
