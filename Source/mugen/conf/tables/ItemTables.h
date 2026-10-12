#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// item_base 表映射的物品 id 区间配置。
struct ItemBaseConfig
{
    // 标识、类型、id 范围、名称类型、堆叠类型和目标表名。
    int32_t id              = 0;
    int32_t itemType        = 0;
    int32_t maxId           = 0;
    int32_t minId           = 0;
    int32_t nameEnglishType = 0;
    int32_t nameType        = 0;
    int32_t stackType       = 0;
    std::string tableName;

    MG_REFLECT(ItemBaseConfig, id, itemType, maxId, minId, nameEnglishType, nameType, stackType, tableName)
};
}  // namespace mugen::conf
