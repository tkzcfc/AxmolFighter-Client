#include "AvatarAssetCache.h"
#include "mugen/core/io/FileUtils.h"

NS_MG_BEGIN

AvatarAssetCache* AvatarAssetCache::s_instance = nullptr;

AvatarAssetCache* AvatarAssetCache::getInstance()
{
    if (s_instance == nullptr)
        s_instance = new AvatarAssetCache();
    return s_instance;
}

void AvatarAssetCache::destroy()
{
    delete s_instance;
    s_instance = nullptr;
}

template <typename T>
std::shared_ptr<const T> AvatarAssetCache::loadCached(std::unordered_map<std::string, std::shared_ptr<const T>>& cache,
                                                      const std::string& path)
{
    const std::string key = io::normalizePath(path);
    const auto it         = cache.find(key);
    if (it != cache.end())
        return it->second;

    auto data = std::make_shared<T>();
    if (data && data->load(key))
    {
        cache[key] = data;
        return data;
    }

    MG_LOG_W("AvatarAssetCache: failed to load '{}'", key);
    return nullptr;
}

std::shared_ptr<const AniData> AvatarAssetCache::getAniData(const std::string& path)
{
    return loadCached(m_aniCache, path);
}

std::shared_ptr<const CombatTimeline> AvatarAssetCache::getCombatTimeline(const std::string& path)
{
    return loadCached(m_boxCache, path);
}

std::shared_ptr<const MotionMap> AvatarAssetCache::getMotionMap(const std::string& path)
{
    return loadCached(m_motionCache, path);
}

void AvatarAssetCache::clear()
{
    m_aniCache.clear();
    m_boxCache.clear();
    m_motionCache.clear();
}

void AvatarAssetCache::invalidate(const std::string& path)
{
    const std::string key = io::normalizePath(path);
    m_aniCache.erase(key);
    m_boxCache.erase(key);
    m_motionCache.erase(key);
}

NS_MG_END
