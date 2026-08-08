#pragma once

#include <vector>
#include "EquipmentModel.h"

namespace game::model
{

struct ItemModel
{
    int64_t id       = 0;  // 物品实例ID
    int64_t configID = 0;  // 物品配置ID
    int32_t count    = 0;  // 数量
};

struct InventoryModel
{
    std::vector<ItemModel> items;            // 消耗品/材料
    std::vector<EquipmentModel> equipments;  // 装备

    void clear()
    {
        items.clear();
        equipments.clear();
    }
};

}  // namespace game::model
