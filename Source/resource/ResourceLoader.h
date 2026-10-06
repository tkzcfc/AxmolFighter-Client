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

// 一批资源的异步加载任务。典型用法：在 gameui::View::onPrepareLoad() 中通过
// gameui::View::getResourceLoader() 拿到一个实例、add() 若干资源；框架会自动 start()
// 并驱动加载界面，不需要手动调用。
//
// 进度：每个顶层资源的权重取 ResourceLoaderRegistry 中该类型的权重；有任务结束时触发 onProgress。
// 失败：失败的资源同样算作结束，进度照常推进，结束时在 onComplete 中汇总。
//
// 生命周期：加载函数被调用过的资源（不限于加载成功）会被 ResourceLoader 持有，
// 析构时立即（同步）对它们调用 releaseHold()。典型用法是让 View 持有一个 ResourceLoader
// 直到自己销毁为止，这样加载完成的资源在整个 View 存活期间都不会被
// SpineSkeletonCache::purgeUnused() 或 FGUIPackageManager::unload() 过早回收；
// 需要让资源比某个 View 活得更久时（例如常驻资源），可以把同一个 ResourceLoader
// 交给多个持有者共同持有（std::shared_ptr），最后一个持有者销毁时才真正释放。
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

    // start() 是否已经被调用过；用于多个持有者共用同一个 ResourceLoader 时避免重复 start()。
    bool isStarted() const { return m_startCalled; }

    // 取消所有未完成的任务，之后不再触发任何回调。
    void cancel();

    float getProgress() const;
    bool isFinished() const;
    // 已结束且失败的顶层资源；配合轮询式的加载流程使用（不依赖 onComplete 回调）。
    std::vector<ResourcePtr> getFailedResources() const;

private:
    ResourcePtr addResource(ResourcePtr resource);
    void unschedule();

    std::shared_ptr<detail::LoaderCore> m_core;
    bool m_scheduled   = false;
    bool m_startCalled = false;
};

}  // namespace gameres
