#include "SpineSkeletonCache.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/Config.h"
#    include "mugen/render/spine/MgSpineLoadPipeline.h"
#    include "mugen/render/spine/MgSpineUtils.h"

#    include <algorithm>
#    include <cstring>

NS_MG_BEGIN

namespace
{

float resSpineScale(const ResSpineConfig* cfg)
{
    return cfg->scale > 0.0f ? cfg->scale : 1.0f;
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
        return it->second;

    auto loaded = MgSkeletonData::loadFromFile(skeletonFile, atlases, scale, false);
    if (loaded)
        m_map.emplace(key, loaded);
    return loaded;
}

MgSkeletonDataPtr SpineSkeletonCache::getOrCreate(int32_t resSpineId)
{
    const auto* cfg = Config::getInstance()->getResSpineConfigById(resSpineId);
    if (!cfg || cfg->spine.empty())
    {
        MG_LOG_E("SpineSkeletonCache::getOrCreate: ResSpine {} missing or spine empty", resSpineId);
        return nullptr;
    }
    return getOrCreate(cfg->spine, {}, resSpineScale(cfg));
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
        onDone(it->second);
        return;
    }

    auto& queue         = m_pending[key];
    const bool inFlight = !queue.empty();
    queue.push_back(std::move(onDone));
    if (inFlight)
        return;

    std::weak_ptr<char> token = m_lifeToken;
    MgSpineLoadPipeline::start(skeletonFile, std::move(atlases), scale, false,
                               [this, token, key](MgSkeletonDataPtr data) {
        if (token.expired())
            return;

        // 在途期间可能已被同步加载写入：以已有数据为准，丢弃本次结果
        if (auto it = m_map.find(key); it != m_map.end())
            data = it->second;
        else if (data)
            m_map.emplace(key, data);

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
    const auto* cfg = Config::getInstance()->getResSpineConfigById(resSpineId);
    if (!cfg || cfg->spine.empty())
    {
        MG_LOG_E("SpineSkeletonCache::loadAsync: ResSpine {} missing or spine empty", resSpineId);
        onDone(nullptr);
        return;
    }
    loadAsync(cfg->spine, {}, resSpineScale(cfg), std::move(onDone));
}

void SpineSkeletonCache::clear()
{
    m_map.clear();
}

void SpineSkeletonCache::purgeUnused()
{
    for (auto it = m_map.begin(); it != m_map.end();)
    {
        if (it->second.use_count() == 1)
            it = m_map.erase(it);
        else
            ++it;
    }
}

NS_MG_END

#endif
