#pragma once

#include <string>
#include <vector>

#include "EquipmentModel.h"

namespace game::model
{

struct DefaultSkinModel
{
    int32_t position     = 0;
    int32_t resFashionId = 0;
};

struct FashionModel
{
    int64_t id       = 0;
    int32_t configID = 0;
    int32_t position = 0;
    bool worn        = false;
};

struct CharacterModel
{
    int64_t characterID = 0;  // 角色ID
    std::string name;         // 角色名
    int32_t classID = 0;      // 职业ID
    int32_t gender  = 0;      // 性别
    int32_t level   = 1;      // 等级
    int64_t exp     = 0;      // 经验值
    int64_t gold    = 0;      // 金币

    std::vector<DefaultSkinModel> defaultSkins;  // 创角默认外观
    std::vector<FashionModel> fashions;          // 拥有的全部装扮
    std::vector<EquipmentModel> equipments;      // 拥有+穿上的全部装备
};

}  // namespace game::model
