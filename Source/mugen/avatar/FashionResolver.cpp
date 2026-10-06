#include "FashionResolver.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/common/TypeConversions.h"
#include "mugen/conf/Config.h"

NS_MG_BEGIN

namespace
{

// 各职业的默认外观（部位 → ResFashion id / 武器 ResSpine id）
std::unordered_map<FashionPosition, int32_t> getDefaultFashion(int32_t roleId)
{
    switch (mugen::type_conversions::toCharacterClass(roleId))
    {
    case mugen::CharacterClass::kSwordman:
    {
        return {
            {FashionPosition::kBody, 101008},  {FashionPosition::kHair, 101004}, {FashionPosition::kClothes, 101002},
            {FashionPosition::kSkin, 101007},  {FashionPosition::kHat, 101001},  {FashionPosition::kFace, 101005},
            {FashionPosition::kWeapon, 14001},
        };
    }
    case mugen::CharacterClass::kRanger:
    {
        return {
            {FashionPosition::kBody, 102008},  {FashionPosition::kHair, 102004}, {FashionPosition::kClothes, 102002},
            {FashionPosition::kSkin, 102007},  {FashionPosition::kHat, 102001},  {FashionPosition::kFace, 102005},
            {FashionPosition::kWeapon, 14101},
        };
    }
    case mugen::CharacterClass::kMage:
    {
        return {
            {FashionPosition::kBody, 104008},  {FashionPosition::kHair, 104004}, {FashionPosition::kClothes, 104002},
            {FashionPosition::kSkin, 104007},  {FashionPosition::kHat, 104001},  {FashionPosition::kFace, 104005},
            {FashionPosition::kWeapon, 14301},
        };
    }
    default:
    {
        return {};
    }
    }
}

// 武器、翅膀、光环是独立的 spine 资源（挂件），不参与身体 atlas 合并
bool isAccessoryPosition(FashionPosition pos)
{
    return pos == FashionPosition::kWeapon || pos == FashionPosition::kWing || pos == FashionPosition::kHalo;
}

}  // namespace

bool FashionResolver::resolve(const FashionAppearance& appearance, AvatarDesc& out)
{
    out = AvatarDesc{};

    const int32_t roleId = appearance.roleId;
    if (roleId <= 0)
    {
        MG_LOG_E("FashionResolver: roleId missing");
        return false;
    }

    const auto* role = Config::getInstance()->getRoleConfigById(roleId);
    if (!role)
    {
        MG_LOG_E("FashionResolver: RoleConfig {} missing", roleId);
        return false;
    }

    const auto* spine = Config::getInstance()->getResSpineConfigById(role->resSpineId);
    if (!spine || spine->spine.empty())
    {
        MG_LOG_E("FashionResolver: ResSpine {} missing for role {}", role->resSpineId, roleId);
        return false;
    }

    // baseFashion 优先，缺省部位用职业默认外观补齐
    std::unordered_map<FashionPosition, int32_t> baseFashion = appearance.baseFashion;
    baseFashion.merge(getDefaultFashion(roleId));

    // 已穿时装：身体部位 → ResFashion id；挂件部位 → ResSpine id
    std::unordered_map<FashionPosition, int32_t> equipBody;
    std::unordered_map<FashionPosition, int32_t> equipAccessory;
    for (const auto& kv : appearance.equipFashion)
    {
        if (kv.second == 0)
            continue;
        const auto* item = Config::getInstance()->getFashionConfigById(kv.second);
        if (!item)
        {
            MG_LOG_W("FashionResolver: FashionConfig {} missing", kv.second);
            continue;
        }
        const auto pos = static_cast<FashionPosition>(item->position);
        if (isAccessoryPosition(pos))
        {
            if (item->spineUiId != 0)
                equipAccessory[pos] = item->spineUiId;
            continue;
        }
        if (item->spineUiId != 0)
            equipBody[pos] = item->spineUiId;
        if (item->spineSkinUiId != 0)
            equipBody[FashionPosition::kSkin] = item->spineSkinUiId;
    }

    const auto pick = [&baseFashion](const std::unordered_map<FashionPosition, int32_t>& equip,
                                     FashionPosition pos) -> int32_t {
        if (auto it = equip.find(pos); it != equip.end())
            return it->second;
        if (auto it = baseFashion.find(pos); it != baseFashion.end())
            return it->second;
        return 0;
    };

    out.skeleton       = spine->spine;
    out.scale          = spine->scale > 0.0f ? spine->scale : 1.0f;
    out.motionFile     = AvatarPaths::motionFileFromSpine(out.skeleton);
    out.characterClass = type_conversions::toCharacterClass(roleId);

    const int32_t haloId = pick(equipAccessory, FashionPosition::kHalo);
    out.setAccessory(AvatarAccessorySlot::kWeapon, pick(equipAccessory, FashionPosition::kWeapon));
    out.setAccessory(AvatarAccessorySlot::kWing, pick(equipAccessory, FashionPosition::kWing));
    out.setAccessory(AvatarAccessorySlot::kHaloBack, haloId);
    out.setAccessory(AvatarAccessorySlot::kHaloFront, haloId);

    for (int i = 0; i < static_cast<int>(FashionPosition::kCount); ++i)
    {
        const auto pos = static_cast<FashionPosition>(i);
        if (isAccessoryPosition(pos))
            continue;

        const int32_t id = pick(equipBody, pos);
        if (id == 0)
            continue;

        const auto* cfg = Config::getInstance()->getResFashionConfigById(id);
        if (!cfg || cfg->spine.empty())
        {
            MG_LOG_W("FashionResolver: ResFashion {} missing for position {}", id, i);
            continue;
        }
        out.atlases.push_back(cfg->spine);
        if (out.skin.empty())
            out.skin = cfg->skinName;
    }

    if (out.atlases.empty())
    {
        MG_LOG_E("FashionResolver: no atlas for role {}", roleId);
        return false;
    }
    return true;
}

NS_MG_END
