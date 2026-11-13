#pragma once

#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mugen::conf
{

// 职业表现：默认装扮、挂件层级与嫁接到身体的插槽名。id 等于 CharacterClass。
struct AvatarClassConfig
{
    // 职业 id，等于 CharacterClass。
    int32_t id = 0;
    // 缺省身体，ResFashion id。0 表示不补，留给角色表。
    int32_t defaultBody = 0;
    // 缺省头发，ResFashion id。0 表示不补。
    int32_t defaultHair = 0;
    // 缺省衣服，ResFashion id。0 表示不补。
    int32_t defaultClothes = 0;
    // 缺省皮肤，ResFashion id。0 表示不补。
    int32_t defaultSkin = 0;
    // 缺省帽子，ResFashion id。0 表示不补。
    int32_t defaultHat = 0;
    // 缺省脸，ResFashion id。0 表示不补。
    int32_t defaultFace = 0;
    // 缺省武器挂件，ResSpine id。0 表示不补。优先于角色表第 9 项。
    int32_t defaultWeapon = 0;
    // 武器挂件相对身体（z=0）的层级。
    int32_t weaponZOrder = 0;
    // 武器嫁接到身体的插槽名；空表示不嫁接。
    std::vector<std::string> weaponGraftSlots;
    // 翅膀挂件相对身体的层级。
    int32_t wingZOrder = 0;
    // 翅膀嫁接到身体的插槽名；空表示不嫁接。
    std::vector<std::string> wingGraftSlots;
    // 光环后层相对身体的层级。
    int32_t haloBackZOrder = 0;
    // 光环前层相对身体的层级。
    int32_t haloFrontZOrder = 0;

    MG_REFLECT(AvatarClassConfig,
               id,
               defaultBody,
               defaultHair,
               defaultClothes,
               defaultSkin,
               defaultHat,
               defaultFace,
               defaultWeapon,
               weaponZOrder,
               weaponGraftSlots,
               wingZOrder,
               wingGraftSlots,
               haloBackZOrder,
               haloFrontZOrder)
};

}  // namespace mugen::conf
