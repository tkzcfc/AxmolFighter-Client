#pragma once

#include "mugen/render/spine/MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

#    include <functional>
#    include <memory>
#    include <string>
#    include <unordered_map>
#    include <vector>

NS_MG_BEGIN

// Spine 骨架数据共享缓存：缓存的数据均为 shared（只读），可换装的身体数据为 exclusive，不进本缓存。
// key = 骨架路径 + 规范化后的 atlas 列表（为空取骨架同名 .atlas）+ scale
class SpineSkeletonCache
{
public:
    using LoadCallback = std::function<void(MgSkeletonDataPtr)>;

    static SpineSkeletonCache* getInstance();
    static void destroy();

    // 同步获取；未缓存则立即加载
    MgSkeletonDataPtr getOrCreate(std::string_view skeletonFile,
                                  const std::vector<std::string>& atlasFiles = {},
                                  float scale                                = 1.0f);
    MgSkeletonDataPtr getOrCreate(int32_t resSpineId);

    // 异步获取；同一 key 的在途请求合并，回调在主线程
    void loadAsync(std::string_view skeletonFile,
                   const std::vector<std::string>& atlasFiles,
                   float scale,
                   LoadCallback onDone);
    void loadAsync(int32_t resSpineId, LoadCallback onDone);

    void clear();
    // 释放只被缓存引用的数据（切换 View 后调用）
    void purgeUnused();

private:
    SpineSkeletonCache();
    ~SpineSkeletonCache();

    SpineSkeletonCache(const SpineSkeletonCache&)            = delete;
    SpineSkeletonCache& operator=(const SpineSkeletonCache&) = delete;

    static std::string makeKey(std::string_view skeletonFile, const std::vector<std::string>& atlasFiles, float scale);

    // 骨架数据缓存
    std::unordered_map<std::string, MgSkeletonDataPtr> m_map;
    // 在途异步请求的回调队列
    std::unordered_map<std::string, std::vector<LoadCallback>> m_pending;
    // 生命周期令牌：异步回调晚于 destroy 时不再访问 this
    std::shared_ptr<char> m_lifeToken;

    static SpineSkeletonCache* s_instance;
};

NS_MG_END

#endif
