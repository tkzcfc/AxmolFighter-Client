#pragma once

#include "mugen/avatar/FashionResolver.h"
#include "net/game_types.pb.h"

namespace gameui
{

// 武装 config_id → 武器挂件 ResSpine id（按职业查 armament 表；0 = 无/回退默认武器）。
int32_t armamentWeaponSpineId(int32_t armamentId, mugen::CharacterClass cls);

// 服务端下发的外观（城镇 / 联网战斗对端）→ 表现层 FashionAppearance：
// class_id→roleId、base_skins→baseFashion、fashions→equipFashion、armament_id→武器挂件 spine。
mugen::FashionAppearance fashionAppearanceFromTown(const PB::Types::TownAppearance& appearance);

// Build FashionAppearance from selected character session + weapon appearance.
mugen::FashionAppearance buildSessionFashionAppearance();

}  // namespace gameui
