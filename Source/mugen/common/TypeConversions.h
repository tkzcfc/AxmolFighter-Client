#pragma once
#include "mugen/core/StdC.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

namespace type_conversions
{

int32_t toRoleConfigId(CharacterClass characterClass);

CharacterClass toCharacterClass(int32_t roleConfigId);

}  // namespace type_conversions

NS_MG_END
