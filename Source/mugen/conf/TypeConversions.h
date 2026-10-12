#pragma once
#include "mugen/core/StdC.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

// 配置 id 与运行时枚举之间的固定映射。
namespace type_conversions
{

// 将职业枚举转换为默认角色配置 id。
int32_t toRoleConfigId(CharacterClass characterClass);

// 将角色配置 id 转换为职业枚举。
CharacterClass toCharacterClass(int32_t roleConfigId);

}  // namespace type_conversions

NS_MG_END
