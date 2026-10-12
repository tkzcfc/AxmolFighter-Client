#include "mugen/conf/TypeConversions.h"

NS_MG_BEGIN

namespace type_conversions
{

// 返回职业对应的默认角色配置 id。
int32_t toRoleConfigId(CharacterClass characterClass)
{
    constexpr int32_t kDefaultHeroRoleId = 101;
    switch (characterClass)
    {
    case CharacterClass::kSwordman:
        return 101;
    case CharacterClass::kRanger:
        return 102;
    case CharacterClass::kFighter:
        return 103;
    case CharacterClass::kMage:
        return 104;
    default:
        break;
    }
    MG_ASSERT(false && "Unknown characterClass");
    return kDefaultHeroRoleId;
}

// 返回角色配置 id 对应的职业，未知 id 返回未知职业。
CharacterClass toCharacterClass(int32_t roleConfigId)
{
    switch (roleConfigId)
    {
    case 101:
        return CharacterClass::kSwordman;
    case 102:
        return CharacterClass::kRanger;
    case 103:
        return CharacterClass::kFighter;
    case 104:
        return CharacterClass::kMage;
    default:
        return CharacterClass::kUnknown;
    }
}

}  // namespace type_conversions

NS_MG_END
