#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/SpineRuntime.h"

NS_MG_BEGIN

class SpineSkeletonCache
{
public:
    static SpineSkeletonCache* getInstance();

    static void destroy();

    // 返回缓存的 SkeletonData（非拥有）；失败返回 nullptr。atlas/loader 仅内部持有。
    MgSkeletonData* getOrCreate(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);
    MgSkeletonData* getOrCreate(std::string_view skeletonFile,
                                const std::vector<std::string>& atlasFiles,
                                float scale = 1.0f);

    MgSkeletonData* getOrCreate(int32_t skeletonId);

    // 预加载
    void preload(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);
    void preload(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale = 1.0f);

    // 按 res_spine 表 id 预加载（查 Config.resSpine）
    bool preloadResSpine(int32_t resSpineId);

    // 清空全部
    void clear();

    // 清除指定条目
    void remove(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);

private:
    struct CacheEntry
    {
        MgSkeletonData* skeletonData        = nullptr;
        MgAtlas* atlas                      = nullptr;
        MgAttachmentLoader* attachmentLoader = nullptr;

        bool valid() const { return skeletonData != nullptr && atlas != nullptr && attachmentLoader != nullptr; }
    };

    SpineSkeletonCache() = default;
    ~SpineSkeletonCache();

    SpineSkeletonCache(const SpineSkeletonCache&)            = delete;
    SpineSkeletonCache& operator=(const SpineSkeletonCache&) = delete;

    CacheEntry load(std::string_view skeletonFile, std::string_view atlasFile, float scale);
    CacheEntry load(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale);
    static void destroyEntry(CacheEntry& entry);
    static CacheEntry readSkeleton(const ax::Data& skelData, MgAtlas* atlas, float scale, std::string_view skeletonFile);

    std::unordered_map<uint64_t, CacheEntry> m_map;

    static SpineSkeletonCache* s_instance;
};

NS_MG_END

#endif
