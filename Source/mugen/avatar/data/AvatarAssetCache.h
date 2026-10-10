#pragma once

#include "AniData.h"
#include "CombatTimeline.h"
#include "MotionMap.h"

#include "mugen/core/reflect/SchemaHash.h"

#include <map>

NS_MG_BEGIN

// 角色资源归档的固定魔数。
inline constexpr std::array<uint8_t, 4> kAvatarMagic{'M', 'G', 'A', 'V'};
// 角色资源归档格式版本。
inline constexpr uint32_t kAvatarVersion = 1;

// 缓存动作图与战斗时间轴资源的进程只读数据。
class AvatarAssetCache
{
public:
    // 返回进程共享的资源缓存实例。
    static AvatarAssetCache* getInstance();
    // 销毁进程共享的资源缓存实例。
    static void destroy();

    // 清空全部已缓存资源和加载状态。
    void clear();
    // 添加资源相对路径的查找目录。
    void addSearchPath(const std::string& path);
    // 清空资源查找目录。
    void clearSearchPaths();
    // 从归档文件加载资源数据。
    bool load(const std::string& path);
    // 返回归档是否已成功加载。
    bool isLoaded() const;
    // 将当前资源数据写入归档文件。
    bool saveToFile(const std::string& path) const;

    // 按路径查询帧动画、战斗时间轴和动作图。
    const AniData* getAniData(const std::string& path) const;
    const CombatTimeline* getCombatTimeline(const std::string& path) const;
    const MotionMap* getMotionMap(const std::string& path) const;

    // 写入帧动画、战斗时间轴和动作图缓存项。
    void putAniData(const std::string& key, const AniData& value);
    void putCombatTimeline(const std::string& key, const CombatTimeline& value);
    void putMotionMap(const std::string& key, const MotionMap& value);

    // 按规范路径索引的帧动画、时间轴和动作图。
    std::map<std::string, AniData> aniDatas;
    std::map<std::string, CombatTimeline> combatTimelines;
    std::map<std::string, MotionMap> motionMaps;

    MG_REFLECT(AvatarAssetCache, aniDatas, combatTimelines, motionMaps)

private:
    // 在路径键映射中查找资源。
    template <typename T>
    const T* findByPath(const std::map<std::string, T>& map, const std::string& path) const;
    // 将动作图片段绑定到对应战斗时间轴。
    bool bindTimelines();

    // 资源相对路径的查找目录。
    std::vector<std::string> m_searchPaths;
    // 当前缓存是否包含有效归档数据。
    bool m_isLoaded = false;
};

// 计算角色资源归档的结构哈希。
inline uint64_t avatarSchemaHash()
{
    return reflect::schemaHash<AvatarAssetCache>();
}

NS_MG_END
