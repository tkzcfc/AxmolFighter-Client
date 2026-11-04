#pragma once

#include "mugen/avatar/FashionResolver.h"

namespace gameui
{

// 武装 config_id → 武器挂件 ResSpine id（按职业查 armament 表；0 = 无/回退默认武器）。
int32_t armamentWeaponSpineId(int32_t armamentId, mugen::CharacterClass cls);

// Build FashionAppearance from selected character session + weapon appearance.
mugen::FashionAppearance buildSessionFashionAppearance();

}  // namespace gameui
