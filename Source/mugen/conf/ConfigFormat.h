#pragma once

#include "ConfigTables.h"
#include "tables/ActionTables.h"
#include "tables/AiTables.h"
#include "tables/BuffTables.h"
#include "tables/EntityTables.h"
#include "tables/EquipTables.h"
#include "tables/FashionTables.h"
#include "tables/ItemTables.h"
#include "tables/MapTables.h"
#include "tables/SoundTables.h"
#include "mugen/core/reflect/SchemaHash.h"

namespace mugen::conf
{
// 配置二进制文件的固定魔数。
inline constexpr std::array<uint8_t, 4> kConfigMagic{'M', 'G', 'C', 'F'};
// 配置二进制格式版本。
inline constexpr uint32_t kConfigVersion = 1;

// 所有注册配置表的归档容器。
struct ConfigArchive
{
    // 各配置表按注册顺序保存的行数据。
#define MG_CONFIG_ARCHIVE_FIELD(kind, Type, Key) std::vector<Type> kind;
    MG_CONFIG_TABLES(MG_CONFIG_ARCHIVE_FIELD)
#undef MG_CONFIG_ARCHIVE_FIELD

    // 反射系统使用的稳定类型名。
    static constexpr std::string_view kMgTypeName = "ConfigArchive";
    // 向可写访问器枚举全部配置表字段。
    // 向只读访问器枚举全部配置表字段。
    template <class V>
    static constexpr void mgReflect(V& visitor, ConfigArchive& self)
    {
#define MG_CONFIG_ARCHIVE_VISIT(kind, Type, Key) visitor.field(#kind, self.kind);
        MG_CONFIG_TABLES(MG_CONFIG_ARCHIVE_VISIT)
#undef MG_CONFIG_ARCHIVE_VISIT
    }
    template <class V>
    static constexpr void mgReflect(V& visitor, const ConfigArchive& self)
    {
#define MG_CONFIG_ARCHIVE_VISIT(kind, Type, Key) visitor.field(#kind, self.kind);
        MG_CONFIG_TABLES(MG_CONFIG_ARCHIVE_VISIT)
#undef MG_CONFIG_ARCHIVE_VISIT
    }
};

// 计算当前配置结构的模式哈希。
inline uint64_t configSchemaHash()
{
    return reflect::schemaHash<ConfigArchive>();
}
}  // namespace mugen::conf
