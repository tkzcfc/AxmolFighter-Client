#pragma once

#include <string>

namespace game::model
{

struct CharacterModel
{
    int64_t characterID = 0;  // 角色ID
    std::string name;         // 角色名
    int32_t classID = 0;      // 职业ID
    int32_t gender  = 0;      // 性别
    int32_t level   = 1;      // 等级
    int64_t exp     = 0;      // 经验值
    int64_t gold    = 0;      // 金币
};

}  // namespace game::model
