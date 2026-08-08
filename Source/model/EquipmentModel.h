#pragma once

#include <vector>
#include <cstdint>

namespace game::model
{

struct EnchantPropModel
{
    int32_t attrID = 0;  // 属性ID
    int32_t value  = 0;  // 属性值
};

struct EquipmentModel
{
    int64_t id           = 0;   // 装备实例唯一ID
    int64_t configID     = 0;   // 装备配置ID
    int32_t enhanceLevel = 0;   // 强化等级
    int32_t refineLevel  = 0;   // 精炼等级
    int32_t slot         = -1;  // 装备槽位（0-5为装备位，-1为背包）

    std::vector<EnchantPropModel> enchantProps;  // 附魔属性列表
};

}  // namespace game::model
