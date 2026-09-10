#pragma once

#include "mugen/render/spine/MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class SpineSkeletonCache
{
public:
    static SpineSkeletonCache* getInstance();
    static void destroy();

    MgSkeletonData* getOrCreate(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);
    MgSkeletonData* getOrCreate(std::string_view skeletonFile,
                                const std::vector<std::string>& atlasFiles,
                                float scale = 1.0f);
    MgSkeletonData* getOrCreate(int32_t skeletonId);

    bool preload(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);
    bool preload(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale = 1.0f);
    bool preload(int32_t resSpineId);

    void clear();
    void remove(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);

private:
    SpineSkeletonCache() = default;
    ~SpineSkeletonCache();

    SpineSkeletonCache(const SpineSkeletonCache&)            = delete;
    SpineSkeletonCache& operator=(const SpineSkeletonCache&) = delete;

    MgSkeletonData* load(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale);

    std::unordered_map<uint64_t, MgSkeletonData*> m_map;
    static SpineSkeletonCache* s_instance;
};

NS_MG_END

#endif
