#pragma once

#include "mugen/render/spine/MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

// Spine 骨架数据共享缓存（atlas 固定的 spine：特效/武器/战斗单位/立绘）。
// 可换装/异步创建的时装身体数据为实例私有，不进本缓存
class SpineSkeletonCache
{
public:
    static SpineSkeletonCache* getInstance();
    static void destroy();

    MgSkeletonData* getOrCreate(std::string_view skeletonFile,
                                const std::vector<std::string>& atlasFiles,
                                float scale = 1.0f);
    MgSkeletonData* getOrCreate(std::string_view skeletonFile, std::string_view atlasFile, float scale = 1.0f);
    MgSkeletonData* getOrCreate(int32_t skeletonId);

    bool preload(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale = 1.0f);
    bool preload(int32_t resSpineId);

    void clear();

private:
    SpineSkeletonCache() = default;
    ~SpineSkeletonCache();

    SpineSkeletonCache(const SpineSkeletonCache&)            = delete;
    SpineSkeletonCache& operator=(const SpineSkeletonCache&) = delete;

    std::unordered_map<uint64_t, MgSkeletonData*> m_map;
    static SpineSkeletonCache* s_instance;
};

NS_MG_END

#endif
