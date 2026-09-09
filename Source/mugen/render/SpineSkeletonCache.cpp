#include "SpineSkeletonCache.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/Config.h"
#    include "xxhash.h"

#    if MG_SPINE_USE_3_4
#        include "spine_3_4/Cocos2dAttachmentLoader.h"
#        include "spine_3_4/extension.h"
#    endif

NS_MG_BEGIN

namespace
{

#    if !MG_SPINE_USE_3_4
static spine::AxmolTextureLoader s_textureLoader;
#    endif

std::string atlasFromSpine(std::string_view spine)
{
    const std::string path(spine);
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + ".atlas";
    return path.substr(0, dot) + ".atlas";
}

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

static uint64_t makeKey(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale)
{
    XXH3_state_t* state = XXH3_createState();
    XXH3_64bits_reset(state);
    XXH3_64bits_update(state, skeletonFile.data(), skeletonFile.size());
    const char sep = 0;
    XXH3_64bits_update(state, &sep, 1);
    for (const auto& atlasFile : atlasFiles)
    {
        XXH3_64bits_update(state, atlasFile.data(), atlasFile.size());
        XXH3_64bits_update(state, &sep, 1);
    }
    XXH3_64bits_update(state, &scale, sizeof(scale));
    const uint64_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    return hash;
}

static MgAtlas* createAtlas(std::string_view atlasFile)
{
    const std::string path(atlasFile);
#    if MG_SPINE_USE_3_4
    MgAtlas* atlas = spAtlas_createFromFile(path.c_str(), 0);
    if (!atlas || !atlas->pages)
    {
        MG_LOG_E("SpineSkeletonCache: failed to read atlas '{}'", atlasFile);
        if (atlas)
            spAtlas_dispose(atlas);
        return nullptr;
    }
    return atlas;
#    else
    auto* atlas = new (__FILE__, __LINE__) spine::Atlas(path.c_str(), &s_textureLoader, true);
    if (!atlas || atlas->getPages().size() == 0)
    {
        MG_LOG_E("SpineSkeletonCache: failed to read atlas '{}'", atlasFile);
        delete atlas;
        return nullptr;
    }
    return atlas;
#    endif
}

static void atlasAppend(MgAtlas* self, std::string_view path)
{
#    if MG_SPINE_USE_3_4
    const std::string file(path);
    spAtlas_append(self, file.c_str());
#    else
    MgAtlas* extra = createAtlas(path);
    if (!extra)
        return;

    auto& extraPages   = extra->getPages();
    auto& extraRegions = extra->getRegions();
    self->getPages().addAll(extraPages);
    self->getRegions().addAll(extraRegions);
    extraPages.clear();
    extraRegions.clear();
    delete extra;
#    endif
}

static MgAtlas* createAtlas(const std::vector<std::string>& atlasFiles)
{
    if (atlasFiles.empty())
        return nullptr;
    MgAtlas* merged = nullptr;
    for (const auto& path : atlasFiles)
    {
        if (!merged)
        {
            merged = createAtlas(std::string_view{path});
            continue;
        }
        atlasAppend(merged, path);
    }
    return merged;
}

}  // namespace

void SpineSkeletonCache::destroyEntry(CacheEntry& entry)
{
#    if MG_SPINE_USE_3_4
    if (entry.skeletonData)
        spSkeletonData_dispose(entry.skeletonData);
    if (entry.attachmentLoader)
        spAttachmentLoader_dispose(entry.attachmentLoader);
    if (entry.atlas)
        spAtlas_dispose(entry.atlas);
#    else
    delete entry.skeletonData;
    delete entry.attachmentLoader;
    delete entry.atlas;
#    endif
    entry.skeletonData     = nullptr;
    entry.attachmentLoader = nullptr;
    entry.atlas            = nullptr;
}

SpineSkeletonCache::CacheEntry SpineSkeletonCache::readSkeleton(const ax::Data& skelData,
                                                                MgAtlas* atlas,
                                                                float scale,
                                                                std::string_view skeletonFile)
{
    CacheEntry out;
    const unsigned char first = firstPayloadByte(skelData);
    const bool isJson         = (first == static_cast<unsigned char>('{'));

#    if MG_SPINE_USE_3_4
    MgAttachmentLoader* attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(atlas));
    MgSkeletonData* skeletonData         = nullptr;
    if (isJson)
    {
        std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                             static_cast<size_t>(skelData.getSize()));
        spSkeletonJson* reader = spSkeletonJson_createWithLoader(attachmentLoader);
        reader->scale          = scale;
        skeletonData           = spSkeletonJson_readSkeletonData(reader, jsonText.c_str());
        if (!skeletonData)
        {
            MG_LOG_E("SpineSkeletonCache: SkeletonJson failed '{}': {}", skeletonFile,
                     reader->error ? reader->error : "unknown");
            spSkeletonJson_dispose(reader);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return out;
        }
        spSkeletonJson_dispose(reader);
    }
    else
    {
        spSkeletonBinary* reader = spSkeletonBinary_createWithLoader(attachmentLoader);
        reader->scale            = scale;
        skeletonData =
            spSkeletonBinary_readSkeletonData(reader, skelData.getBytes(), static_cast<int>(skelData.getSize()));
        if (!skeletonData)
        {
            MG_LOG_E("SpineSkeletonCache: SkeletonBinary failed '{}': {}", skeletonFile,
                     reader->error ? reader->error : "unknown");
            spSkeletonBinary_dispose(reader);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return out;
        }
        spSkeletonBinary_dispose(reader);
    }
    out.skeletonData     = skeletonData;
    out.atlas            = atlas;
    out.attachmentLoader = attachmentLoader;
#    else
    auto* attachmentLoader            = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
    spine::SkeletonData* skeletonData = nullptr;
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
#    endif
    return out;
}

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

MgSkeletonData* SpineSkeletonCache::getOrCreate(std::string_view skeletonFile, std::string_view atlasFile, float scale)
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

MgSkeletonData* SpineSkeletonCache::getOrCreate(std::string_view skeletonFile,
                                                const std::vector<std::string>& atlasFiles,
                                                float scale)
{
    if (atlasFiles.size() <= 1)
        return getOrCreate(skeletonFile, atlasFiles.empty() ? std::string_view{} : atlasFiles.front(), scale);

    const uint64_t key = makeKey(skeletonFile, atlasFiles, scale);
    auto it            = m_map.find(key);
    if (it != m_map.end())
        return it->second.skeletonData;

    CacheEntry loaded = load(skeletonFile, atlasFiles, scale);
    if (!loaded.valid())
        return nullptr;

    m_map[key] = loaded;
    return loaded.skeletonData;
}

MgSkeletonData* SpineSkeletonCache::getOrCreate(int32_t skeletonId)
{
    auto* cfg = Config::getInstance()->getResSpineConfigById(skeletonId);
    if (!cfg || cfg->spine.empty())
    {
        MG_LOG_E("SpineSkeletonCache::getOrCreate: ResSpine {} missing or spine empty", skeletonId);
        return nullptr;
    }
    const float scale       = cfg->scale > 0.0f ? cfg->scale : 1.0f;
    const std::string atlas = "";
    return getOrCreate(cfg->spine, atlas, scale);
}

void SpineSkeletonCache::preload(std::string_view skeletonFile, std::string_view atlasFile, float scale)
{
    if (skeletonFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache::preload: empty skeleton path");
        return;
    }
    if (!ax::FileUtils::getInstance()->isFileExist(skeletonFile))
    {
        MG_LOG_E("SpineSkeletonCache::preload: skeleton missing '{}'", skeletonFile);
        return;
    }
    getOrCreate(skeletonFile, atlasFile, scale);
}

void SpineSkeletonCache::preload(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale)
{
    getOrCreate(skeletonFile, atlasFiles, scale);
}

bool SpineSkeletonCache::preloadResSpine(int32_t resSpineId)
{
    auto* cfg = Config::getInstance()->getResSpineConfigById(resSpineId);
    if (!cfg || cfg->spine.empty())
    {
        MG_LOG_E("SpineSkeletonCache::preloadResSpine: ResSpine {} missing or spine empty", resSpineId);
        return false;
    }
    const float scale       = cfg->scale > 0.0f ? cfg->scale : 1.0f;
    const std::string atlas = "";
    preload(cfg->spine, atlas, scale);
    return getOrCreate(cfg->spine, atlas, scale) != nullptr;
}

void SpineSkeletonCache::clear()
{
    for (auto& pair : m_map)
        destroyEntry(pair.second);
    m_map.clear();
}

void SpineSkeletonCache::remove(std::string_view skeletonFile, std::string_view atlasFile, float scale)
{
    const uint64_t key = makeKey(skeletonFile, atlasFile, scale);
    auto it            = m_map.find(key);
    if (it == m_map.end())
        return;

    destroyEntry(it->second);
    m_map.erase(it);
}

SpineSkeletonCache::CacheEntry SpineSkeletonCache::load(std::string_view skeletonFile,
                                                        std::string_view atlasFile,
                                                        float scale)
{
    CacheEntry out;
    if (skeletonFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache: empty skeleton path");
        return out;
    }

    ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(skeletonFile);
    if (skelData.isNull() || skelData.getSize() <= 0)
    {
        MG_LOG_E("SpineSkeletonCache: failed to read skeleton '{}'", skeletonFile);
        return out;
    }

    std::string atlasPath(atlasFile);
    if (atlasPath.empty())
        atlasPath = atlasFromSpine(skeletonFile);
    auto* atlas = createAtlas(atlasPath);
    if (!atlas)
        return out;

    return readSkeleton(skelData, atlas, scale, skeletonFile);
}

SpineSkeletonCache::CacheEntry SpineSkeletonCache::load(std::string_view skeletonFile,
                                                        const std::vector<std::string>& atlasFiles,
                                                        float scale)
{
    if (atlasFiles.size() <= 1)
        return load(skeletonFile, atlasFiles.empty() ? std::string_view{} : atlasFiles.front(), scale);

    CacheEntry out;
    if (skeletonFile.empty())
    {
        MG_LOG_E("SpineSkeletonCache: empty skeleton path");
        return out;
    }

    ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(skeletonFile);
    if (skelData.isNull() || skelData.getSize() <= 0)
    {
        MG_LOG_E("SpineSkeletonCache: failed to read skeleton '{}'", skeletonFile);
        return out;
    }

    auto* atlas = createAtlas(atlasFiles);
    if (!atlas)
        return out;

    return readSkeleton(skelData, atlas, scale, skeletonFile);
}

NS_MG_END

#endif
