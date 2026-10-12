#pragma once

#include "BTDef.h"
#include <map>

namespace mugen
{
// 缓存生命周期必须覆盖全部 runner，期间配置保持不可变。
// 组件仅保存 Key 与 BTInstance，新缓存会在 get() 时重建定义。
template <class Key, class Param, class Config>
class BTDefCache
{
public:
    // 根据键和只读配置构建树定义的函数类型。
    using Builder = BTDef<Param>(const Key&, const Config&);
    // 绑定只读配置与构建函数。
    BTDefCache(const Config& config, Builder& builder) : m_config(config), m_builder(&builder) {}
    // 返回已缓存定义，首次访问时按键构建。
    const BTDef<Param>& get(const Key& key)
    {
        const auto found = m_defs.find(key);
        if (found != m_defs.end())
            return found->second;
        return m_defs.emplace(key, m_builder(key, m_config)).first->second;
    }

private:
    // 缓存期内不可变的源配置。
    const Config& m_config;
    // 用于延迟构建定义的非空函数指针。
    Builder* m_builder;
    // 按键稳定保存已构建的共享定义。
    std::map<Key, BTDef<Param>> m_defs;
};
}  // namespace mugen
