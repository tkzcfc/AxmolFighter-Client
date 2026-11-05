#include "ui/progress/SessionFashionAppearance.h"

#include "AppContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/tables/ArmamentTables.h"
#include "ui/widgets/weapon/WeaponLocalState.h"

namespace gameui
{

int32_t armamentWeaponSpineId(int32_t armamentId, mugen::CharacterClass cls)
{
    if (armamentId <= 0)
        return 0;
    const auto* cfg = mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>(
        {armamentId, static_cast<int32_t>(cls)});
    if (!cfg || cfg->spineId <= 0)
        return 0;
    return cfg->spineId;
}

mugen::FashionAppearance fashionAppearanceFromTown(const PB::Types::TownAppearance& appearance)
{
    mugen::FashionAppearance fa;
    const auto cls = static_cast<mugen::CharacterClass>(appearance.class_id());
    fa.roleId      = mugen::type_conversions::toRoleConfigId(cls);
    for (int i = 0; i < appearance.base_skins_size(); ++i)
    {
        const auto& skin = appearance.base_skins(i);
        if (skin.res_fashion_id() <= 0)
            continue;
        fa.baseFashion[static_cast<mugen::FashionPosition>(skin.position())] = skin.res_fashion_id();
    }
    for (int i = 0; i < appearance.fashions_size(); ++i)
    {
        const auto& item = appearance.fashions(i);
        if (item.config_id() <= 0)
            continue;
        fa.equipFashion[static_cast<mugen::FashionPosition>(item.position())] = item.config_id();
    }
    const int32_t weapon = armamentWeaponSpineId(appearance.armament_id(), cls);
    if (weapon > 0)
        fa.baseFashion[mugen::FashionPosition::kWeapon] = weapon;
    return fa;
}

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
            const int32_t weapon = armamentWeaponSpineId(appId, cls);
            if (weapon > 0)
                appearance.baseFashion[mugen::FashionPosition::kWeapon] = weapon;
        }
    }
    return appearance;
}

}  // namespace gameui
