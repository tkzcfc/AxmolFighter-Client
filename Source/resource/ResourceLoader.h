#pragma once

#include "resource/Resource.h"

#include "base/Macros.h"

#include <functional>
#include <memory>
#include <vector>

namespace gameres
{

namespace detail
{
class LoaderCore;
}  // namespace detail

struct LoadProgress
{
    float percent        = 0.0f;  // 0..1，按类型权重加权
    size_t finishedCount = 0;     // 已结束（含失败）的顶层资源数
    size_t totalCount    = 0;     // 顶层资源总数
};

// 一批资源的异步加载任务。典型用法：在 IResourceLoadOperation::start() 中创建、
// add() 若干资源、调用 start()；在 update() 里根据 isFinished() 切换到正式内容。
//
//   m_loader = std::make_unique<gameres::ResourceLoader>();
//   m_loader->add<gameres::FguiPackageResource>("UI/Town");
//   m_loader->add<gameres::SpineResource>(resSpineId);
//   m_loader->start(
//       [this](const gameres::LoadProgress& p) { m_progressBar->setValue(p.percent * 100.0); },
//       [this](const std::vector<gameres::ResourcePtr>& failed) { ... });
//
// 进度：每个顶层资源的权重取 ResourceLoaderRegistry 中该类型的权重；有任务结束时触发 onProgress。
// 失败：失败的资源同样算作结束，进度照常推进，结束时在 onComplete 中汇总。
//
// 生命周期：加载函数已经被调用过的资源（不限于加载成功）会被 ResourceLoader 持有，
// 析构时延迟到下一帧才调用它们的 releaseHold()。这样刚创建的正式内容可以先引用这些资源
// （FGUI 包引用计数、Spine 共享数据 use_count），避免被 SpineSkeletonCache::purgeUnused()
// 或 FGUIPackageManager::unload() 过早回收。
class ResourceLoader
{
public:
    ResourceLoader();
    ~ResourceLoader();

    ResourceLoader(const ResourceLoader&)            = delete;
    ResourceLoader& operator=(const ResourceLoader&) = delete;

    // 添加一个资源，只能在 start() 之前调用。按“类型 + getKey()”去重，
    // 重复添加时返回已存在的实例。
    template <class T, class... Args>
    std::shared_ptr<T> add(Args&&... args)
    {
        auto result = addResource(std::make_shared<T>(std::forward<Args>(args)...));
        AXASSERT(dynamic_cast<T*>(result.get()),
                 "ResourceLoader::add: resource type/key already used by another class");
        return std::static_pointer_cast<T>(result);
    }

    // 同时处于加载中的任务数上限，默认 8；避免大量纹理在同一帧内一起创建 Texture2D 造成卡顿。
    void setMaxConcurrent(size_t maxConcurrent);

    // 每帧在主线程执行加载工作（启动任务、组合资源收尾、runOnWorker 的 then）的时间预算，默认 8ms。
    // 超出预算的工作顺延到下一帧；每帧至少执行一项。设为 0 表示每帧只执行一项。
    void setFrameBudget(float milliseconds);

    using ProgressCallback = std::function<void(const LoadProgress&)>;
    using CompleteCallback = std::function<void(const std::vector<ResourcePtr>& failed)>;

    // 只能调用一次。回调都在主线程触发，且不会在 start() 的调用栈内同步触发。
    void start(ProgressCallback onProgress, CompleteCallback onComplete);

    // 取消所有未完成的任务，之后不再触发任何回调。
    void cancel();

    float getProgress() const;
    bool isFinished() const;

private:
    ResourcePtr addResource(ResourcePtr resource);
    void unschedule();

    std::shared_ptr<detail::LoaderCore> m_core;
    bool m_scheduled = false;
};

}  // namespace gameres
