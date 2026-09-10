#include "FashionSpine.h"
#include "mugen/common/TypeConversions.h"
#include "mugen/conf/Config.h"

NS_MG_BEGIN

std::unordered_map<FashionPosition, int32_t> getDefaultFashion(int32_t roleId)
{
    switch (mugen::type_conversions::toCharacterClass(roleId))
    {
    case mugen::CharacterClass::kSwordman:
    {
        return {
            {FashionPosition::kBody, 101008},
            {FashionPosition::kHair, 101004},
            {FashionPosition::kClothes, 101002},
            {FashionPosition::kSkin, 101007},
            {FashionPosition::kHat, 101001},
            {FashionPosition::kFace, 101005},
            {FashionPosition::kWeapon, 14001},
        };
    }
    case mugen::CharacterClass::kRanger:
    {
        return {{FashionPosition::kBody, 102008},
                {FashionPosition::kHair, 102004},
                {FashionPosition::kClothes, 102002},
                {FashionPosition::kSkin, 102007},
                {FashionPosition::kHat, 102001},
                {FashionPosition::kFace, 102005},
                {FashionPosition::kWeapon, 14101},
        };
    }
    case mugen::CharacterClass::kMage:
    {
        return {{FashionPosition::kBody, 104008},            
                {FashionPosition::kHair, 104004},
                {FashionPosition::kClothes, 104002},
                {FashionPosition::kSkin, 104007},
                {FashionPosition::kHat, 104001},
                {FashionPosition::kFace, 104005},
                {FashionPosition::kWeapon, 14301},
        };
    }
    default:
    {
        return {};
    }
    }
}

FashionSpineDesc FashionResolver::resolve(const FashionAppearance& appearance)
{
    FashionSpineDesc desc;
    desc.atlasSlotIndex.fill(-1);

    const int32_t roleId = appearance.roleId;
    if (roleId <= 0)
    {
        MG_LOG_E("FashionResolver: roleId missing");
        return desc;
    }

    const auto* role = Config::getInstance()->getRoleConfigById(roleId);
    if (!role)
    {
        MG_LOG_E("FashionResolver: RoleConfig {} missing", roleId);
        return desc;
    }

    std::unordered_map<FashionPosition, int32_t> baseFashion = appearance.baseFashion;
    baseFashion.merge(getDefaultFashion(roleId));

    std::unordered_map<FashionPosition, int32_t> equipSpineIds;
    std::vector<const FashionConfig*> equipItems;

    for (const auto& kv : appearance.equipFashion)
    {
        if (kv.second == 0)
            continue;
        const auto* item = Config::getInstance()->getFashionConfigById(kv.second);
        if (!item)
            continue;
        equipItems.push_back(item);
    }

    // 武器
    int32_t fashionWeaponId = 0;
    // 翅膀
    int32_t fashionWingId = 0;
    // 光环
    int32_t fashionRingId = 0;

    // TODO: 套装光环
    //if (!equipItems.empty())
    //    fashionRingId = ringIdFromEquip(equipItems);

    for (const auto* item : equipItems)
    {
        const auto pos = static_cast<FashionPosition>(item->position);
        
        // 武器
        if (pos == FashionPosition::kWeapon)
        {
            fashionWeaponId = item->id;
        }
        // 翅膀
        else if (pos == FashionPosition::kWing)
        {
            if (item->spineUiId != 0)
                fashionWingId = item->spineUiId;
            continue;
        }
        // 光环
        else if (pos == FashionPosition::kHalo)
        {
            if (item->spineUiId != 0)
                fashionRingId = item->spineUiId;
            continue;
        }
        else
        {
            if (item->spineUiId != 0)
                equipSpineIds[static_cast<FashionPosition>(pos)] = item->spineUiId;
            if (item->spineSkinUiId != 0)
                equipSpineIds[FashionPosition::kSkin] = item->spineSkinUiId;
        }
    }

    const auto* spine = Config::getInstance()->getResSpineConfigById(role->resSpineId);
    if (!spine || spine->spine.empty())
    {
        MG_LOG_E("FashionResolver: ResSpine {} missing for role {}", role->resSpineId, roleId);
        return desc;
    }

    desc.skeleton = spine->spine;
    desc.scale         = spine->scale > 0.0f ? spine->scale : 1.0f;
    desc.weaponSpineId = fashionWeaponId;
    desc.wingSpineId   = fashionWingId;
    desc.ringSpineId   = fashionRingId;

    for (int i = static_cast<int>(FashionPosition::kBody); i < static_cast<int>(FashionPosition::kCount); ++i)
    {
        // 武器、翅膀、光环是独立的spine资源，不需要在这里处理
        if (i == static_cast<int>(FashionPosition::kWeapon) ||
            i == static_cast<int>(FashionPosition::kWing) ||
            i == static_cast<int>(FashionPosition::kHalo))
        {
            continue;
        }

        int32_t id    = 0;
        auto it = equipSpineIds.find(static_cast<FashionPosition>(i));
        if (it == equipSpineIds.end())
        {
            it = baseFashion.find(static_cast<FashionPosition>(i));
            if (it != baseFashion.end())
            {
                id = it->second;
            }
        }
        else
        {
            id = it->second;
        }

        if (id == 0)
            continue;

        const auto* cfg = Config::getInstance()->getResFashionConfigById(id);
        if (cfg && !cfg->spine.empty())
        {
            desc.atlasSlotIndex[i] = static_cast<int32_t>(desc.atlases.size());
            desc.atlases.push_back(cfg->spine);

            if (desc.skin.empty())
            {
                desc.skin = cfg->skinName;                    
            }
        }
    }
    desc.valid         = !desc.skeleton.empty() && !desc.atlases.empty();
    if (!desc.valid)
        MG_LOG_E("FashionResolver: no atlas for role {}", roleId);
    return desc;
}

NS_MG_END
