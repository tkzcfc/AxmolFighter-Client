#pragma once

#include "ConfigTables.h"
#include "mugen/core/reflect/SchemaHash.h"

namespace mugen::conf
{
inline constexpr std::array<uint8_t, 4> kConfigMagic{'M', 'G', 'C', 'F'};
inline constexpr uint32_t kConfigVersion = 1;

struct ConfigArchive
{
#define MG_CONFIG_ARCHIVE_FIELD(kind, Type, Key) std::vector<Type> kind;
    MG_CONFIG_TABLES(MG_CONFIG_ARCHIVE_FIELD)
#undef MG_CONFIG_ARCHIVE_FIELD

    static constexpr std::string_view kMgTypeName = "ConfigArchive";
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

inline uint64_t configSchemaHash()
{
    return reflect::schemaHash<ConfigArchive>();
}
}  // namespace mugen::conf
