#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// fashion 表映射的时装配置。
struct FashionConfig
{
    // 获取、激活属性、基础属性和类别参数。
    std::vector<int32_t> accessTypeId;
    std::vector<int32_t> activeAttrType;
    std::vector<int32_t> activeAttrValue;
    std::vector<int32_t> activeAttrValueType;
    std::vector<int32_t> attributeType;
    std::vector<int32_t> attributeValue;
    std::vector<int32_t> bigType;
    // 交易、状态、文本、销毁与展示参数。
    int32_t bourseId        = 0;
    int32_t buff            = 0;
    int32_t collect         = 0;
    int32_t descId          = 0;
    int32_t destoryId       = 0;
    int32_t destoryNum      = 0;
    int32_t destoryType     = 0;
    int32_t discard         = 0;
    int32_t dropEffect      = 0;
    // 模板、物品、图标、标识和时长参数。
    int32_t fashionTemplate = 0;
    int32_t goodsId         = 0;
    std::string icon;
    int32_t id       = 0;
    int32_t lastTime = 0;
    std::variant<int32_t, std::string> miniIcon;
    // 文本、职业、部位、品质、稀有度与评分参数。
    int32_t nameId      = 0;
    int32_t occupation  = 0;
    int32_t phantomId   = 0;
    int32_t position    = 0;
    int32_t quality     = 0;
    int32_t rarityBasic = 0;
    std::vector<int32_t> rarityStar;
    int32_t roleLevel = 0;
    int32_t score     = 0;
    int32_t shield    = 0;
    std::vector<int32_t> showItem;
    // 特殊能力、UI Spine、升星、套装和时间参数。
    int32_t specialability  = 0;
    int32_t spineSkinUiId   = 0;
    int32_t spineUiId       = 0;
    int32_t starupMaterials = 0;
    int32_t starupNum       = 0;
    int32_t suitId          = 0;
    int32_t timeType        = 0;

    MG_REFLECT(FashionConfig,
               accessTypeId,
               activeAttrType,
               activeAttrValue,
               activeAttrValueType,
               attributeType,
               attributeValue,
               bigType,
               bourseId,
               buff,
               collect,
               descId,
               destoryId,
               destoryNum,
               destoryType,
               discard,
               dropEffect,
               fashionTemplate,
               goodsId,
               icon,
               id,
               lastTime,
               miniIcon,
               nameId,
               occupation,
               phantomId,
               position,
               quality,
               rarityBasic,
               rarityStar,
               roleLevel,
               score,
               shield,
               showItem,
               specialability,
               spineSkinUiId,
               spineUiId,
               starupMaterials,
               starupNum,
               suitId,
               timeType)
};

// fashion_suit 表映射的时装套装配置。
struct FashionSuitConfig
{
    // 标识、列表图、文本、职业、护盾、部件和品质。
    int32_t id = 0;
    std::string listPic;
    int32_t nameId     = 0;
    int32_t occupation = 0;
    int32_t shield     = 0;
    std::vector<int32_t> suitParts;
    int32_t suitQuality = 0;

    MG_REFLECT(FashionSuitConfig, id, listPic, nameId, occupation, shield, suitParts, suitQuality)
};

// res_fashion 表映射的时装资源配置。
struct ResFashionConfig
{
    // 标识、皮肤名称和 Spine 资源路径。
    int32_t id = 0;
    std::variant<int32_t, std::string> skinName;
    std::variant<int32_t, std::string> spine;

    MG_REFLECT(ResFashionConfig, id, skinName, spine)
};
}  // namespace mugen::conf
