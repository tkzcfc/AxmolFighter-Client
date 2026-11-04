#include "SpineSkeletonCache.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/ReflectedConfig.h"
#    include "mugen/render/spine/MgSpineLoadPipeline.h"
#    include "mugen/render/spine/MgSpineUtils.h"

#    include "axmol.h"

#    include <algorithm>
#    include <cstring>
#    include <set>

NS_MG_BEGIN

namespace
{

float resSpineScale(const conf::ResSpineConfig* cfg)
{
    return cfg->scale > Fixed{} ? cfg->scale.toFloatForRender() : 1.0f;
}

}  // namespace

SpineSkeletonCache* SpineSkeletonCache::s_instance = nullptr;

SpineSkeletonCache* SpineSkeletonCache::getInstance()
{
    if (!s_instance)
        s_instance = new SpineSkeletonCache();
    return s_instance;
}

void SpineSkeletonCache::destroy()
{
    delete s_instance;
    s_instance = nullptr;
}

SpineSkeletonCache::SpineSkeletonCache() : m_lifeToken(std::make_shared<char>(0)) {}

SpineSkeletonCache::~SpineSkeletonCache()
{
    m_lifeToken.reset();
    clear();
}

std::string SpineSkeletonCache::makeKey(std::string_view skeletonFile,
                                        const std::vector<std::string>& atlasFiles,
                                        float scale)
{
    std::string key(skeletonFile);
    std::replace(key.begin(), key.end(), '\\', '/');
    for (const auto& atlasFile : atlasFiles)
    {
        key += '|';
        key += atlasFile;
    }
    uint32_t scaleBits = 0;
    std::memcpy(&scaleBits, &scale, sizeof(scaleBits));
    key += '|';
    key += std::to_string(scaleBits);
    return key;
}

void SpineSkeletonCache::store(const std::string& key, MgSkeletonDataPtr data, const std::vector<std::string>& atlases)
{
    CacheEntry entry;
    entry.data          = std::move(data);
    entry.texturePaths  = collectTexturePaths(atlases);
    m_map.emplace(key, std::move(entry));
}

void SpineSkeletonCache::releaseOrphanTextures(const std::vector<std::string>& texturePaths)
{
    auto* cache = ax::Director::getInstance()->getTextureCache();
    std::set<std::string> unique(texturePaths.begin(), texturePaths.end());
    for (const auto& path : unique)
    {
        auto* texture = cache->getTextureForKey(path);
        if (texture && texture->getReferenceCount() == 1)
            cache->removeTextureForKey(path);
    }
}

MgSkeletonDataPtr SpineSkeletonCache::getOrCreate(std::string_view skeletonFile,
                                                  const std::vector<std::string>& atlasFiles,
                                                  float scale)
{
    if (skeletonFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache::getOrCreate: empty skeleton path");
        return nullptr;
    }
    const auto atlases = resolveAtlasFiles(skeletonFile, atlasFiles);
    const auto key     = makeKey(skeletonFile, atlases, scale);
    if (auto it = m_map.find(key); it != m_map.end())
        return it->second.data;

    auto loaded = MgSkeletonData::loadFromFile(skeletonFile, atlases, scale, false);
    if (loaded)
        store(key, loaded, atlases);
    return loaded;
}

MgSkeletonDataPtr SpineSkeletonCache::getOrCreate(int32_t resSpineId)
{
    const auto* cfg = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    if (!cfg || cfg->fileName.empty())
    {
        MG_LOG_E("SpineSkeletonCache::getOrCreate: ResSpine {} missing or spine empty", resSpineId);
        return nullptr;
    }
    return getOrCreate(cfg->fileName, {}, resSpineScale(cfg));
}

void SpineSkeletonCache::loadAsync(std::string_view skeletonFile,
                                   const std::vector<std::string>& atlasFiles,
                                   float scale,
                                   LoadCallback onDone)
{
    if (skeletonFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache::loadAsync: empty skeleton path");
        onDone(nullptr);
        return;
    }
    auto atlases   = resolveAtlasFiles(skeletonFile, atlasFiles);
    const auto key = makeKey(skeletonFile, atlases, scale);
    if (auto it = m_map.find(key); it != m_map.end())
    {
        onDone(it->second.data);
        return;
    }

    auto& queue         = m_pending[key];
    const bool inFlight = !queue.empty();
    queue.push_back(std::move(onDone));
    if (inFlight)
        return;

    std::weak_ptr<char> token = m_lifeToken;
    MgSpineLoadPipeline::start(skeletonFile, atlases, scale, false,
                               [this, token, key, atlases](MgSkeletonDataPtr data) {
        if (token.expired())
            return;

        // 在途期间可能已被同步加载写入：以已有数据为准，丢弃本次结果
        if (auto it = m_map.find(key); it != m_map.end())
            data = it->second.data;
        else if (data)
            store(key, data, atlases);

        auto it = m_pending.find(key);
        if (it == m_pending.end())
            return;
        auto callbacks = std::move(it->second);
        m_pending.erase(it);
        for (auto& callback : callbacks)
            callback(data);
    });
}

void SpineSkeletonCache::loadAsync(int32_t resSpineId, LoadCallback onDone)
{
    const auto* cfg = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    if (!cfg || cfg->fileName.empty())
    {
        MG_LOG_E("SpineSkeletonCache::loadAsync: ResSpine {} missing or spine empty", resSpineId);
        onDone(nullptr);
        return;
    }
    loadAsync(cfg->fileName, {}, resSpineScale(cfg), std::move(onDone));
}

void SpineSkeletonCache::clear()
{
    std::vector<std::string> textures;
    for (auto& [_, entry] : m_map)
        textures.insert(textures.end(), entry.texturePaths.begin(), entry.texturePaths.end());
    m_map.clear();
    releaseOrphanTextures(textures);
}

void SpineSkeletonCache::purgeUnused()
{
    std::vector<std::string> textures;
    for (auto it = m_map.begin(); it != m_map.end();)
    {
        if (it->second.data.use_count() == 1)
        {
            textures.insert(textures.end(), it->second.texturePaths.begin(), it->second.texturePaths.end());
            it = m_map.erase(it);
        }
        else
            ++it;
    }
    releaseOrphanTextures(textures);
}

NS_MG_END

#endif
