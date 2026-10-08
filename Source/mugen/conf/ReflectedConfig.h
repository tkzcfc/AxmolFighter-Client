#pragma once

#include "ConfigFormat.h"

namespace mugen::conf
{
class Config
{
public:
    bool load(const std::string& path);
    bool loadBytes(std::vector<uint8_t> bytes);
    bool loaded() const { return m_loaded; }

    template <class T>
    const T* find(int32_t key) const
    {
        const auto& rows = table<T>();
        const auto it =
            std::lower_bound(rows.begin(), rows.end(), key, [](const T& row, int32_t value) { return row.id < value; });
        return it != rows.end() && it->id == key ? &*it : nullptr;
    }

    template <class T>
    const T& get(int32_t key) const
    {
        const T* value = find<T>(key);
        if (!value)
            MG_LOG_E("Config row not found: type={} key={}", T::kMgTypeName, key);
        MG_ASSERT(value);
        return *value;
    }

    template <class T>
    const std::vector<T>& table() const;

private:
    ConfigArchive m_archive;
    bool m_loaded = false;
};

#define MG_CONFIG_TABLE_ACCESSOR(kind, Type, Key)               \
    template <>                                                 \
    inline const std::vector<Type>& Config::table<Type>() const \
    {                                                           \
        return m_archive.kind;                                  \
    }
MG_CONFIG_TABLES(MG_CONFIG_TABLE_ACCESSOR)
#undef MG_CONFIG_TABLE_ACCESSOR
}  // namespace mugen::conf
