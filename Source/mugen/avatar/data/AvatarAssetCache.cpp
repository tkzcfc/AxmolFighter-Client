#include "AvatarAssetCache.h"

#include "mugen/core/io/FileUtils.h"
#include "mugen/core/serialize/ByteBuffer.h"

#include <algorithm>
#include <cstdio>

NS_MG_BEGIN

namespace
{
AvatarAssetCache* s_instance = nullptr;

std::string trimSlash(std::string path)
{
    path = io::normalizePath(std::move(path));
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}
}  // namespace

AvatarAssetCache::AvatarAssetCache() {}
AvatarAssetCache::~AvatarAssetCache() {}

AvatarAssetCache* AvatarAssetCache::getInstance()
{
    if (!s_instance)
        s_instance = new (std::nothrow) AvatarAssetCache();
    return s_instance;
}

void AvatarAssetCache::destroy()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void AvatarAssetCache::clear()
{
    m_isLoaded = false;
    aniDatas.clear();
    combatTimelines.clear();
    motionMaps.clear();
}

void AvatarAssetCache::addSearchPath(const std::string& path)
{
    std::string normalized = trimSlash(path);
    if (normalized.empty())
        return;
    m_searchPaths.erase(std::remove(m_searchPaths.begin(), m_searchPaths.end(), normalized), m_searchPaths.end());
    m_searchPaths.push_back(std::move(normalized));
}

void AvatarAssetCache::clearSearchPaths()
{
    m_searchPaths.clear();
}

bool AvatarAssetCache::load(const std::string& path)
{
    clear();

    size_t size  = 0;
    uint8_t* raw = io::getFileData(path, size);
    if (raw == nullptr || size == 0)
    {
        MG_LOG_E("Failed to load avatar assets: {}", path);
        return false;
    }

    ByteBuffer buffer;
    buffer.fastSet(raw, static_cast<uint32_t>(size));
    if (!deserialize(buffer))
    {
        MG_LOG_E("AvatarAssetCache deserialize failed: {}", path);
        std::fprintf(stderr, "AvatarAssetCache deserialize failed: %s\n", path.c_str());
        clear();
        return false;
    }

    MG_LOG_I("Loaded avatar assets: ani={} box={} motion={}", aniDatas.size(), combatTimelines.size(),
             motionMaps.size());
    m_isLoaded = true;
    return true;
}

bool AvatarAssetCache::isLoaded() const
{
    return m_isLoaded;
}

bool AvatarAssetCache::saveToFile(const std::string& path) const
{
    ByteBuffer buffer;
    serialize(buffer);
    buffer.writeFinish();
    return io::writeDataToFile(reinterpret_cast<const char*>(buffer.data()), buffer.len(), path);
}

void AvatarAssetCache::putAniData(const std::string& key, const AniData& value)
{
    aniDatas[io::normalizePath(key)] = value;
}

void AvatarAssetCache::putCombatTimeline(const std::string& key, const CombatTimeline& value)
{
    combatTimelines[io::normalizePath(key)] = value;
}

void AvatarAssetCache::putMotionMap(const std::string& key, const MotionMap& value)
{
    motionMaps[io::normalizePath(key)] = value;
}

template <typename T>
const T* AvatarAssetCache::findByPath(const std::unordered_map<std::string, T>& map, const std::string& path) const
{
    const std::string key = io::normalizePath(path);
    auto it               = map.find(key);
    if (it != map.end())
        return &it->second;

    for (const std::string& searchPath : m_searchPaths)
    {
        it = map.find(searchPath + "/" + key);
        if (it != map.end())
            return &it->second;
    }
    return nullptr;
}

const AniData* AvatarAssetCache::getAniData(const std::string& path) const
{
    return findByPath(aniDatas, path);
}

const CombatTimeline* AvatarAssetCache::getCombatTimeline(const std::string& path) const
{
    return findByPath(combatTimelines, path);
}

const MotionMap* AvatarAssetCache::getMotionMap(const std::string& path) const
{
    return findByPath(motionMaps, path);
}

void AvatarAssetCache::serializeCustomImpl(ByteBuffer&) const {}

bool AvatarAssetCache::deserializeCustomImpl(ByteBuffer&)
{
    for (auto& kv : motionMaps)
    {
        if (!kv.second.bindTimelines([this](const std::string& boxPath) { return getCombatTimeline(boxPath); }))
        {
#if defined(OLUA_AUTOCONF)
            std::fprintf(stderr, "AvatarAssetCache: bind motion failed '%s'\n", kv.first.c_str());
#else
            MG_LOG_E("AvatarAssetCache: bind motion failed '{}'", kv.first);
#endif
            return false;
        }
    }
    return true;
}

NS_MG_END
