#pragma once

#include "BTDef.h"
#include <map>

namespace mugen
{
// Cache lifetime encloses all runners. Configuration is immutable for that lifetime.
// The component stores Key plus BTInstance only; a fresh cache rebuilds on get().
template <class Key, class Param, class Config>
class BTDefCache
{
public:
    using Builder = BTDef<Param>(const Key&, const Config&);
    BTDefCache(const Config& config, Builder& builder) : m_config(config), m_builder(&builder) {}
    const BTDef<Param>& get(const Key& key)
    {
        const auto found = m_defs.find(key);
        if (found != m_defs.end())
            return found->second;
        return m_defs.emplace(key, m_builder(key, m_config)).first->second;
    }

private:
    const Config& m_config;
    Builder* m_builder;
    std::map<Key, BTDef<Param>> m_defs;
};
}  // namespace mugen
