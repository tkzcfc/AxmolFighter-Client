#include "resource/ResourceLoaderRegistry.h"
#include "resource/builtin/BuiltinLoaders.h"

#include <algorithm>

namespace gameres
{

ResourceLoaderRegistry& ResourceLoaderRegistry::getInstance()
{
    static ResourceLoaderRegistry instance = [] {
        ResourceLoaderRegistry r;
        registerBuiltinLoaders(r);
        return r;
    }();
    return instance;
}

void ResourceLoaderRegistry::registerRaw(ResourceType type, LoaderFn fn, float weight)
{
    m_entries[type] = Entry{std::move(fn), std::max(weight, 0.0f)};
}

void ResourceLoaderRegistry::setWeight(ResourceType type, float weight)
{
    auto it = m_entries.find(type);
    if (it != m_entries.end())
        it->second.weight = std::max(weight, 0.0f);
}

float ResourceLoaderRegistry::getWeight(ResourceType type) const
{
    auto it = m_entries.find(type);
    return it != m_entries.end() ? it->second.weight : 1.0f;
}

const ResourceLoaderRegistry::LoaderFn* ResourceLoaderRegistry::find(ResourceType type) const
{
    auto it = m_entries.find(type);
    return it != m_entries.end() ? &it->second.fn : nullptr;
}

}  // namespace gameres
