#include "ui/progress/SessionFashionAppearance.h"

#include "AppContext.h"
#include "mugen/conf/TypeConversions.h"
#include "ui/widgets/weapon/WeaponLocalState.h"

namespace gameui
{

mugen::FashionAppearance buildSessionFashionAppearance()
{
    mugen::FashionAppearance appearance;
    auto* session = AppContext::get().gameSession();
    if (!session || session->selectedCharacter.characterID == 0)
        return appearance;

    const auto cls    = static_cast<mugen::CharacterClass>(session->selectedCharacter.classID);
    appearance.roleId = mugen::type_conversions::toRoleConfigId(cls);

    for (const auto& skin : session->selectedCharacter.defaultSkins)
    {
        if (skin.resFashionId <= 0)
            continue;
        appearance.baseFashion[static_cast<mugen::FashionPosition>(skin.position)] = skin.resFashionId;
    }
    for (const auto& f : session->selectedCharacter.fashions)
    {
        if (!f.worn || f.configID <= 0)
            continue;
        appearance.equipFashion[static_cast<mugen::FashionPosition>(f.position)] = f.configID;
    }

    auto* weapons = AppContext::get().weaponLocalState();
    if (weapons)
    {
        const int32_t appId = weapons->appearanceId();
        if (appId > 0)
        {
            if (const auto* st = weapons->find(appId); st && st->skinId > 0)
                appearance.baseFashion[mugen::FashionPosition::kWeapon] = st->skinId;
        }
    }
    return appearance;
}

}  // namespace gameui
