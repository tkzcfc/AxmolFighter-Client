#include "SpineSkeletonCache.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/Config.h"
#    include "mugen/render/spine/MgSpineUtils.h"
#    include "xxhash.h"

NS_MG_BEGIN

namespace
{

uint64_t makeKey(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale)
{
    XXH3_state_t* state = XXH3_createState();
    XXH3_64bits_reset(state);
    XXH3_64bits_update(state, skeletonFile.data(), skeletonFile.size());
    const char sep = 0;
    XXH3_64bits_update(state, &sep, 1);
    for (const auto& atlasFile : atlasFiles)
    {
        if (!atlasFile.empty())
            XXH3_64bits_update(state, atlasFile.data(), atlasFile.size());
        XXH3_64bits_update(state, &sep, 1);
    }
    XXH3_64bits_update(state, &scale, sizeof(scale));
    const uint64_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    return hash;
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

SpineSkeletonCache::~SpineSkeletonCache()
{
    clear();
}

MgSkeletonData* SpineSkeletonCache::getOrCreate(std::string_view skeletonFile,
                                                const std::vector<std::string>& atlasFiles,
                                                float scale)
{
    if (skeletonFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache::getOrCreate: empty skeleton path");
        return nullptr;
    }
    const uint64_t key = makeKey(skeletonFile, atlasFiles, scale);
    auto it            = m_map.find(key);
    if (it != m_map.end())
        return it->second;

    MgSkeletonData* loaded = MgSkeletonData::loadFromFile(skeletonFile, atlasFiles, scale);
    if (!loaded)
        return nullptr;

    m_map[key] = loaded;
    return loaded;
}

MgSkeletonData* SpineSkeletonCache::getOrCreate(std::string_view skeletonFile, std::string_view atlasFile, float scale)
{
    return getOrCreate(skeletonFile, std::vector<std::string>{std::string(atlasFile)}, scale);
}

MgSkeletonData* SpineSkeletonCache::getOrCreate(int32_t skeletonId)
{
    auto* cfg = Config::getInstance()->getResSpineConfigById(skeletonId);
    if (!cfg || cfg->spine.empty())
    {
        MG_LOG_E("SpineSkeletonCache::getOrCreate: ResSpine {} missing or spine empty", skeletonId);
        return nullptr;
    }
    const float scale = cfg->scale > 0.0f ? cfg->scale : 1.0f;
    return getOrCreate(cfg->spine, std::vector<std::string>{}, scale);
}

bool SpineSkeletonCache::preload(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale)
{
    return getOrCreate(skeletonFile, atlasFiles, scale) != nullptr;
}

bool SpineSkeletonCache::preload(int32_t resSpineId)
{
    return getOrCreate(resSpineId) != nullptr;
}

void SpineSkeletonCache::clear()
{
    for (auto& pair : m_map)
        delete pair.second;
    m_map.clear();
}

NS_MG_END

#endif
