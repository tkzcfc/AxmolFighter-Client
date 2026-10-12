#pragma once

#include "ConfigFormat.h"

namespace mugen::conf
{
// 验证归档的规则名称，拒绝未注册的配置类。
bool validateConfigArchive(const ConfigArchive& archive);

// 指定配置行类型的查找键策略。
template <class T>
struct ConfigTablePolicy;

#define MG_CONFIG_POLICY(kind, Type, Key) \
    template <>                           \
    struct ConfigTablePolicy<Type>        \
    {                                     \
        using Policy = Key;               \
    };
MG_CONFIG_TABLES(MG_CONFIG_POLICY)
#undef MG_CONFIG_POLICY

// 根据表策略推导配置行的查找键类型。
template <class T>
using ConfigKey = std::conditional_t<std::is_same_v<typename ConfigTablePolicy<T>::Policy, KeyByIdAndOccupation>,
                                     std::pair<int32_t, int32_t>,
                                     int32_t>;

// 运行时已加载配置表的只读查询入口。
class Config
{
public:
    // 返回进程共享的配置实例。
    static Config& instance();

    // 从文件读取并验证配置归档。
    bool load(const std::string& path);
    // 从内存字节读取并验证配置归档。
    bool loadBytes(std::vector<uint8_t> bytes);
    bool loaded() const { return m_loaded; }

    // 按表策略查找行，缺失时返回空指针。
    template <class T>
    const T* find(ConfigKey<T> key) const
    {
        const auto& rows = table<T>();
        using Policy     = typename ConfigTablePolicy<T>::Policy;
        if constexpr (std::is_same_v<Policy, KeyByOrdinal>)
        {
            // Ordinals retain the source Lua array's one-based identity.
            return key > 0 && static_cast<size_t>(key) <= rows.size() ? &rows[static_cast<size_t>(key - 1)] : nullptr;
        }
        else
        {
            const auto it =
                std::lower_bound(rows.begin(), rows.end(), key,
                                 [](const T& row, const ConfigKey<T>& value) { return Policy::get(row) < value; });
            return it != rows.end() && Policy::get(*it) == key ? &*it : nullptr;
        }
    }

    // 按表策略取得必需行，缺失时记录并断言。
    template <class T>
    const T& get(ConfigKey<T> key) const
    {
        const T* value = find<T>(key);
        if (!value)
        {
            if constexpr (std::is_same_v<typename ConfigTablePolicy<T>::Policy, KeyByIdAndOccupation>)
                MG_LOG_E("Config row not found: type={} id={} occupation={}", T::kMgTypeName, key.first, key.second);
            else
                MG_LOG_E("Config row not found: type={} key={}", T::kMgTypeName, key);
        }
        MG_ASSERT(value);
        return *value;
    }

    // 为 id 表提供语义明确的可空查找。
    template <class T>
        requires std::is_same_v<typename ConfigTablePolicy<T>::Policy, KeyById>
    const T* findById(int32_t id) const
    {
        return find<T>(id);
    }

    // 为 id 表提供语义明确的必需查找。
    template <class T>
        requires std::is_same_v<typename ConfigTablePolicy<T>::Policy, KeyById>
    const T& getById(int32_t id) const
    {
        return get<T>(id);
    }

    // 返回指定类型的完整配置行列表。
    template <class T>
    const std::vector<T>& table() const;

private:
    // 已通过验证的配置归档。
    ConfigArchive m_archive;
    // 归档是否已成功加载。
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
