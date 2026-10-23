#pragma once

#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>

namespace mugen::conf
{

// Slim armament (武装 / GodWeapon artifact) row for client UI / local cheat state.
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
    // Star-1 spine_ui_id from item_artifact_star (ResSpine id + anim name).
    std::string spineAnim;
    int32_t spineId = 0;
    // res_video id + baked path (empty if no skill video).
    int32_t video = 0;
    std::string videoPath;

    MG_REFLECT(ArmamentConfig,
               comboSkill,
               descId,
               icon,
               iconBig,
               iconList,
               id,
               nameId,
               occupation,
               quality,
               skin,
               spineAnim,
               spineId,
               video,
               videoPath)
};

}  // namespace mugen::conf
