#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// equip 表映射的装备配置。
struct EquipConfig
{
    // 属性类型和值。
    std::vector<int32_t> attributeType;
    std::vector<int32_t> attributeValue;
    // 交易、状态、芯片、文本和销毁参数。
    int32_t bourseId = 0;
    int32_t buffId   = 0;
    int32_t chipId   = 0;
    int32_t descId   = 0;
    std::variant<int32_t, std::vector<int32_t>> destoryId;
    std::variant<int32_t, std::vector<int32_t>> destoryNum;
    int32_t destoryType = 0;
    int32_t discard     = 0;
    // 附魔、闪烁、锻造及宝石参数。
    int32_t enchantType = 0;
    int32_t flickerRate = 0;
    int32_t forgeId     = 0;
    std::vector<int32_t> gemLevLimit;
    std::vector<int32_t> gemOpenLev;
    std::vector<int32_t> gemType;
    // 关联物品、图标、标识和强化参数。
    int32_t goodsId = 0;
    std::string icon;
    int32_t id = 0;
    std::vector<int32_t> intensifyBreakLev;
    // 时长、等级、职业、部位、品质和随机属性参数。
    int32_t lastTime               = 0;
    int32_t level                  = 0;
    int32_t maxLevel               = 0;
    int32_t nameId                 = 0;
    int32_t occupation             = 0;
    int32_t position               = 0;
    int32_t quality                = 0;
    int32_t randAttributeNum       = 0;
    int32_t randAttributeRefinedId = 0;
    // 精炼道具、材料、属性和权重参数。
    int32_t refinedItemId          = 0;
    int32_t refinedItemNum         = 0;
    std::vector<int32_t> refinedMaterials;
    std::vector<int32_t> refinedMaterialsNum;
    std::variant<int32_t, std::vector<int32_t>> refinedValueMax;
    std::vector<int32_t> refinedbuffId;
    std::vector<int32_t> refinedbuffLv;
    int32_t refinedbuffNum = 0;
    std::vector<int32_t> refinedbuffWeight;
    std::variant<int32_t, std::vector<int32_t>> resetRandAttribute;
    std::variant<int32_t, std::vector<int32_t>> resetRandAttributeNum;
    // 排序、特殊能力、Spine、套装与升级参数。
    int32_t sortWeight          = 0;
    int32_t specialAbilityId    = 0;
    int32_t specialAttributeId  = 0;
    int32_t specialAttributeNum = 0;
    std::vector<int32_t> spineId;
    int32_t suitId        = 0;
    int32_t timeType      = 0;
    int32_t updataItemId  = 0;
    int32_t updataItemNum = 0;
    int32_t updateTimes   = 0;

    MG_REFLECT(EquipConfig,
               attributeType,
               attributeValue,
               bourseId,
               buffId,
               chipId,
               descId,
               destoryId,
               destoryNum,
               destoryType,
               discard,
               enchantType,
               flickerRate,
               forgeId,
               gemLevLimit,
               gemOpenLev,
               gemType,
               goodsId,
               icon,
               id,
               intensifyBreakLev,
               lastTime,
               level,
               maxLevel,
               nameId,
               occupation,
               position,
               quality,
               randAttributeNum,
               randAttributeRefinedId,
               refinedItemId,
               refinedItemNum,
               refinedMaterials,
               refinedMaterialsNum,
               refinedValueMax,
               refinedbuffId,
               refinedbuffLv,
               refinedbuffNum,
               refinedbuffWeight,
               resetRandAttribute,
               resetRandAttributeNum,
               sortWeight,
               specialAbilityId,
               specialAttributeId,
               specialAttributeNum,
               spineId,
               suitId,
               timeType,
               updataItemId,
               updataItemNum,
               updateTimes)
};
}  // namespace mugen::conf
