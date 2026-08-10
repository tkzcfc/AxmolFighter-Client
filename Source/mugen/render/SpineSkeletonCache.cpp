#include "SpineSkeletonCache.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/Config.h"
#    include "xxhash.h"

NS_MG_BEGIN

namespace
{
static spine::AxmolTextureLoader s_textureLoader;

// Skip UTF-8 BOM and ASCII whitespace; return first payload byte or 0 if empty.
static unsigned char firstPayloadByte(const ax::Data& data)
{
    const unsigned char* bytes = data.getBytes();
    size_t size                = static_cast<size_t>(data.getSize());
    size_t i                   = 0;
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        i = 3;
    while (i < size)
    {
        const unsigned char c = bytes[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            ++i;
            continue;
        }
        return c;
    }
    return 0;
}

static uint64_t makeKey(std::string_view skeletonFile, std::string_view atlasFile, float scale)
{
    XXH3_state_t* state = XXH3_createState();
    XXH3_64bits_reset(state);
    XXH3_64bits_update(state, skeletonFile.data(), skeletonFile.size());
    const char sep = 0;
    XXH3_64bits_update(state, &sep, 1);
    XXH3_64bits_update(state, atlasFile.data(), atlasFile.size());
    XXH3_64bits_update(state, &sep, 1);
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

spine::SkeletonData* SpineSkeletonCache::getOrCreate(std::string_view skeletonFile,
                                                     std::string_view atlasFile,
                                                     float scale)
{
    const uint64_t key = makeKey(skeletonFile, atlasFile, scale);
    auto it            = m_map.find(key);
    if (it != m_map.end())
        return it->second.skeletonData;

    CacheEntry loaded = load(skeletonFile, atlasFile, scale);
    if (!loaded.valid())
        return nullptr;

    m_map[key] = loaded;
    return loaded.skeletonData;
}

void SpineSkeletonCache::preload(std::string_view skeletonFile, std::string_view atlasFile, float scale)
{
    if (skeletonFile.empty() || atlasFile.empty())
        return;
    if (!ax::FileUtils::getInstance()->isFileExist(skeletonFile))
    {
        MG_LOG_E("SpineSkeletonCache::preload: skeleton missing '{}'", skeletonFile);
        return;
    }
    getOrCreate(skeletonFile, atlasFile, scale);
}

bool SpineSkeletonCache::preloadResSpine(int32_t resSpineId)
{
    auto* cfg = Config::getInstance()->getResSpineConfigById(resSpineId);
    if (!cfg || cfg->spine.empty() || cfg->atlas.empty())
    {
        MG_LOG_E("SpineSkeletonCache::preloadResSpine: ResSpine {} missing or atlas empty", resSpineId);
        return false;
    }
    const float scale = cfg->scale > 0.0f ? cfg->scale : 1.0f;
    preload(cfg->spine, cfg->atlas, scale);
    return getOrCreate(cfg->spine, cfg->atlas, scale) != nullptr;
}

void SpineSkeletonCache::clear()
{
    for (auto& pair : m_map)
    {
        delete pair.second.skeletonData;
        delete pair.second.attachmentLoader;
        delete pair.second.atlas;
    }
    m_map.clear();
}

void SpineSkeletonCache::remove(std::string_view skeletonFile, std::string_view atlasFile, float scale)
{
    const uint64_t key = makeKey(skeletonFile, atlasFile, scale);
    auto it            = m_map.find(key);
    if (it == m_map.end())
        return;

    delete it->second.skeletonData;
    delete it->second.attachmentLoader;
    delete it->second.atlas;
    m_map.erase(it);
}

SpineSkeletonCache::CacheEntry SpineSkeletonCache::load(std::string_view skeletonFile,
                                                        std::string_view atlasFile,
                                                        float scale)
{
    CacheEntry out;
    if (skeletonFile.empty() || atlasFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache: empty skeleton/atlas path");
        return out;
    }

    ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(skeletonFile);
    if (skelData.isNull() || skelData.getSize() <= 0)
    {
        MG_LOG_E("SpineSkeletonCache: failed to read skeleton '{}'", skeletonFile);
        return out;
    }

    // Atlas 需要以 '\0' 结尾的 C 字符串；string_view 不保证终止符
    const std::string atlasPath(atlasFile);
    auto* atlas = new (__FILE__, __LINE__) spine::Atlas(atlasPath.c_str(), &s_textureLoader, true);
    if (!atlas || atlas->getPages().size() == 0)
    {
        MG_LOG_E("SpineSkeletonCache: failed to read atlas '{}'", atlasFile);
        delete atlas;
        return out;
    }

    auto* attachmentLoader            = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
    spine::SkeletonData* skeletonData = nullptr;
    const unsigned char first         = firstPayloadByte(skelData);
    const bool isJson                 = (first == static_cast<unsigned char>('{'));

    if (isJson)
    {
        std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                             static_cast<size_t>(skelData.getSize()));
        spine::SkeletonJson reader(attachmentLoader);
        reader.setScale(scale);
        skeletonData = reader.readSkeletonData(jsonText.c_str());
        if (!skeletonData)
        {
            MG_LOG_E("SpineSkeletonCache: SkeletonJson failed '{}': {}", skeletonFile,
                     reader.getError().isEmpty() ? "unknown" : reader.getError().buffer());
            delete attachmentLoader;
            delete atlas;
            return out;
        }
    }
    else
    {
        spine::SkeletonBinary reader(attachmentLoader);
        reader.setScale(scale);
        skeletonData = reader.readSkeletonData(skelData.getBytes(), static_cast<int>(skelData.getSize()));
        if (!skeletonData)
        {
            MG_LOG_E("SpineSkeletonCache: SkeletonBinary failed '{}': {}", skeletonFile,
                     reader.getError().isEmpty() ? "unknown" : reader.getError().buffer());
            delete attachmentLoader;
            delete atlas;
            return out;
        }
    }

    out.skeletonData     = skeletonData;
    out.atlas            = atlas;
    out.attachmentLoader = attachmentLoader;
    return out;
}

NS_MG_END

#endif
