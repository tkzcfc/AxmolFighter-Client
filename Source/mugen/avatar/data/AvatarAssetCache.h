#pragma once

#include "AniData.h"
#include "CombatTimeline.h"
#include "MotionMap.h"

NS_MG_BEGIN

// 动画数据、战斗时间轴、动作映射的缓存管理器
class AvatarAssetCache
{
public:
    static AvatarAssetCache* getInstance();
    static void destroy();

    std::shared_ptr<const AniData> getAniData(const std::string& path);
    std::shared_ptr<const CombatTimeline> getCombatTimeline(const std::string& path);
    std::shared_ptr<const MotionMap> getMotionMap(const std::string& path);

    // 清空所有缓存
    void clear();

    // 使指定路径的缓存失效，下次访问会重新加载
    void invalidate(const std::string& path);

private:
    AvatarAssetCache() = default;

    AvatarAssetCache(const AvatarAssetCache&)            = delete;
    AvatarAssetCache& operator=(const AvatarAssetCache&) = delete;

    template <typename T>
    std::shared_ptr<const T> loadCached(std::unordered_map<std::string, std::shared_ptr<const T>>& cache,
                                        const std::string& path);

    static AvatarAssetCache* s_instance;

    // .ani 缓存
    std::unordered_map<std::string, std::shared_ptr<const AniData>> m_aniCache;
    // .box 缓存
    std::unordered_map<std::string, std::shared_ptr<const CombatTimeline>> m_boxCache;
    // .motion 缓存
    std::unordered_map<std::string, std::shared_ptr<const MotionMap>> m_motionCache;
};

NS_MG_END
