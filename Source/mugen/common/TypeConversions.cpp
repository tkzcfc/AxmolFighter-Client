#include "TypeConversions.h"

NS_MG_BEGIN

namespace type_conversions
{

int32_t toRoleConfigId(CharacterClass characterClass)
{
    constexpr int32_t kDefaultHeroRoleId = 101;
    switch (characterClass)
    {
    case CharacterClass::kSwordman:
        return 101;
    case CharacterClass::kRanger:
        return 102;
    case CharacterClass::kMage:
        return 103;
    default:
        break;
    }
    MG_ASSERT(false && "Unknown characterClass");
    return kDefaultHeroRoleId;
}

CharacterClass toCharacterClass(int32_t roleConfigId)
{
    switch (roleConfigId)
    {
    case 101:
        return CharacterClass::kSwordman;
    case 102:
        return CharacterClass::kRanger;
    case 103:
        return CharacterClass::kMage;
    default:
        return CharacterClass::kUnknown;
    }
}

}  // namespace type_conversions

NS_MG_END
