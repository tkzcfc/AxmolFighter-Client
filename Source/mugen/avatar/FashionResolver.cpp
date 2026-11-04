#include "FashionResolver.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/ReflectedConfig.h"

#include <unordered_set>

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

// 伙伴职业（风/女王/狂暴）走伙伴时装替换规则。
bool isPartnerCharacterClass(CharacterClass cls)
{
    return cls == CharacterClass::kPartnerWind || cls == CharacterClass::kPartnerQueen ||
           cls == CharacterClass::kPartnerRage;
}

// 解析时装图集路径：支持单 atlas 或双形态 atlas 列表。
std::string pickResFashionAtlas(const conf::ResFashionConfig& cfg, bool transformed)
{
    if (const auto* atlas = std::get_if<std::string>(&cfg.spine))
        return *atlas;

    const auto* atlasList = std::get_if<std::vector<std::string>>(&cfg.spine);
    if (!atlasList || atlasList->empty())
        return {};

    const size_t index = transformed ? 1u : 0u;
    if (index < atlasList->size() && !(*atlasList)[index].empty())
        return (*atlasList)[index];
    if (!atlasList->front().empty())
        return atlasList->front();
    return {};
}

// 角色表的 resFashion 使用固定槽位：第 8 项是身体图集，第 9 项是武器挂件。
void mergeRoleFallbackFashion(const conf::RoleConfig& role, std::unordered_map<FashionPosition, int32_t>& out)
{
    if (role.resFashion.size() >= 8)
    {
        const int32_t bodyId = role.resFashion[7];
        if (bodyId > 0 && !out.contains(FashionPosition::kBody))
            out.emplace(FashionPosition::kBody, bodyId);
    }
    if (role.resFashion.size() >= 9)
    {
        const int32_t weaponId = role.resFashion[8];
        if (weaponId > 0 && !out.contains(FashionPosition::kWeapon))
            out.emplace(FashionPosition::kWeapon, weaponId);
    }
}

}  // namespace

// 将外观选择解析为身体图集、皮肤和挂件描述。
bool FashionResolver::resolve(const FashionAppearance& appearance, AvatarDesc& out)
{
    out = AvatarDesc{};

    const int32_t roleId = appearance.roleId;
    if (roleId <= 0)
    {
        MG_LOG_E("FashionResolver: roleId missing");
        return false;
    }

    const auto* role = conf::Config::instance().find<conf::RoleConfig>(roleId);
    if (!role)
    {
        MG_LOG_E("FashionResolver: RoleConfig {} missing", roleId);
        return false;
    }

    out.characterClass = type_conversions::toCharacterClass(roleId);

    int32_t resSpineId = role->resSpineId;
    if (appearance.transformed && role->resSpineIdExt > 0)
        resSpineId = role->resSpineIdExt;

    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    if (!spine || spine->fileName.empty())
    {
        MG_LOG_E("FashionResolver: ResSpine {} missing for role {}", resSpineId, roleId);
        return false;
    }

    // baseFashion 优先，缺省部位用角色表默认与职业默认补齐
    std::unordered_map<FashionPosition, int32_t> baseFashion = appearance.baseFashion;
    mergeRoleFallbackFashion(*role, baseFashion);
    baseFashion.merge(getDefaultFashion(roleId));

    // 已穿时装：身体部位 → ResFashion id；挂件部位 → ResSpine id
    std::unordered_map<FashionPosition, int32_t> equipBody;
    std::unordered_map<FashionPosition, int32_t> equipAccessory;
    for (const auto& kv : appearance.equipFashion)
    {
        if (kv.second == 0)
            continue;
        const auto* item = conf::Config::instance().find<conf::FashionConfig>(
            {kv.second, static_cast<int32_t>(out.characterClass)});
        if (!item)
            item = conf::Config::instance().find<conf::FashionConfig>({kv.second, 0});
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

    out.skeleton       = spine->fileName;
    out.scale          = spine->scale > Fixed{} ? spine->scale.toFloatForRender() : 1.0f;
    out.motionFile     = AvatarPaths::motionFileFromSpine(out.skeleton);

    const int32_t haloId = pick(equipAccessory, FashionPosition::kHalo);
    int32_t weaponId = pick(equipAccessory, FashionPosition::kWeapon);
    if (appearance.transformed && out.characterClass == CharacterClass::kPartnerRage)
        weaponId = 0;
    out.setAccessory(AvatarAccessorySlot::kWeapon, weaponId);
    out.setAccessory(AvatarAccessorySlot::kWing, pick(equipAccessory, FashionPosition::kWing));
    out.setAccessory(AvatarAccessorySlot::kHaloBack, haloId);
    out.setAccessory(AvatarAccessorySlot::kHaloFront, haloId);

    const bool partnerClass = isPartnerCharacterClass(out.characterClass);
    const bool hasClothesEquipped =
        equipBody.find(FashionPosition::kClothes) != equipBody.end();
    std::unordered_set<std::string> atlasSet;

    for (int i = 0; i < static_cast<int>(FashionPosition::kCount); ++i)
    {
        const auto pos = static_cast<FashionPosition>(i);
        if (isAccessoryPosition(pos))
            continue;
        if (partnerClass && hasClothesEquipped && pos == FashionPosition::kBody)
            continue;

        const int32_t id = pick(equipBody, pos);
        if (id == 0)
            continue;

        const auto* cfg = conf::Config::instance().find<conf::ResFashionConfig>(id);
        if (!cfg)
        {
            MG_LOG_W("FashionResolver: ResFashion {} missing for position {}", id, i);
            continue;
        }
        const std::string atlas = pickResFashionAtlas(*cfg, appearance.transformed);
        if (atlas.empty())
        {
            MG_LOG_W("FashionResolver: ResFashion {} missing for position {}", id, i);
            continue;
        }
        if (atlasSet.emplace(atlas).second)
            out.atlases.push_back(atlas);
        if (out.skin.empty())
        {
            if (const auto* skin = std::get_if<std::string>(&cfg->skinName))
                out.skin = *skin;
        }
    }

    if (out.atlases.empty())
    {
        MG_LOG_E("FashionResolver: no atlas for role {}", roleId);
        return false;
    }
    return true;
}

NS_MG_END
