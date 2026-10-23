#include "FeatureEntry.h"

#include "AppContext.h"
#include "ui/widgets/fashion/FashionDressPanel.h"
#include "ui/widgets/fashion/FashionMainPanel.h"
#include "ui/widgets/fashion/FashionTypes.h"
#include "ui/widgets/weapon/WeaponMainPanel.h"
#include "ui/widgets/weapon/WeaponTypes.h"

namespace gameui
{

namespace
{
const char* toString(FeatureId id)
{
    switch (id)
    {
    case FeatureId::Dress:
        return "Dress";
    case FeatureId::Weapon:
        return "Weapon";
    case FeatureId::Equip:
        return "Equip";
    case FeatureId::Partner:
        return "Partner";
    case FeatureId::Skill:
        return "Skill";
    }
    return "Unknown";
}
}  // namespace

std::weak_ptr<UIWidget> FeatureEntry::open(FeatureId id, std::any params)
{
    auto* uiManager = AppContext::get().uiManager();
    if (!uiManager)
    {
        AXLOGE("FeatureEntry: UIManager is null, cannot open {}", toString(id));
        return {};
    }

    switch (id)
    {
    case FeatureId::Dress:
    {
        FashionOpenParams openParams;
        if (params.has_value())
        {
            if (params.type() == typeid(FashionOpenParams))
                openParams = std::any_cast<FashionOpenParams>(params);
            else if (params.type() == typeid(mugen::FashionPosition))
            {
                openParams.openDress    = true;
                openParams.initialPart  = std::any_cast<mugen::FashionPosition>(params);
            }
            else
                AXLOGW("FeatureEntry: Dress received unexpected params type");
        }

        if (openParams.openDress)
            return uiManager->open<FashionDressPanel>(openParams.initialPart);
        return uiManager->open<FashionMainPanel>();
    }
    case FeatureId::Weapon:
    {
        // Optional WeaponOpenParams currently unused beyond open; focus later.
        return uiManager->open<WeaponMainPanel>();
    }
    case FeatureId::Equip:
    case FeatureId::Partner:
    case FeatureId::Skill:
        AXLOGI("FeatureEntry: {} not implemented", toString(id));
        return {};
    }

    AXLOGE("FeatureEntry: unhandled FeatureId {}", static_cast<int>(id));
    return {};
}

}  // namespace gameui
