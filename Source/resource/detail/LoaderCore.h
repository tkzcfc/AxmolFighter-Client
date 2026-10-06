#pragma once

// 内部实现细节：只给 ResourceLoader.cpp 和 LoadTask.cpp 使用。
// 资源类型的实现只应该依赖 Resource.h / LoadTask.h / ResourceLoaderRegistry.h。
//
// 本类不依赖 Director/Scheduler：按帧驱动由 ResourceLoader 调用 tick()，
// 这里只是纯粹的调度状态机，方便脱离引擎做单元测试。

#include "resource/Resource.h"
#include "resource/ResourceLoader.h"
#include "resource/ResourceLoaderRegistry.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gameres::detail
{

struct Entry
{
    ResourcePtr resource;

    // 只有 root 条目（ResourceLoader::add 添加的）计入总权重；子条目（LoadTask::spawn 派生的）
    // 不计入总权重，它们的完成情况被聚合为父条目的进度。
    bool isRoot  = false;
    float weight = 1.0f;

    bool started  = false;  // 加载函数是否已经被调用过
    bool finished = false;
    bool success  = false;

    // 是否占用一个并发名额。条目一旦 spawn 子条目就让出名额，只等子条目完成，
    // 否则等子条目的父条目占满名额时子条目永远排不上号。
    bool holdsSlot = false;

    std::vector<Entry*> children;  // 用于聚合进度
    std::vector<Entry*> parents;   // 去重后一个子条目可能被多个父条目共享

    bool childrenNotified = false;
    std::function<void()> onChildrenDoneFn;
};

// 单个条目的 0..1 进度：已完成为 1；有子条目时取子条目进度的平均值（递归）；否则为 0。
float computeEntryProgress(const Entry& e);

// root 条目的加权总进度：Σ(weight_i * progress_i) / Σ(weight_i)；
// 总权重 <= 0 时退化为按已完成个数计算。
float computeWeightedProgress(const std::vector<Entry*>& roots);

// ResourceLoader 的内部状态。LoadTask 只持有 weak_ptr<LoaderCore>，
// ResourceLoader 销毁或 cancel 之后所有异步回调都会安全地变成空操作。
class LoaderCore : public std::enable_shared_from_this<LoaderCore>
{
public:
    explicit LoaderCore(gameres::ResourceLoaderRegistry& registry = gameres::ResourceLoaderRegistry::getInstance());

    ResourcePtr addRoot(ResourcePtr resource);
    void setMaxConcurrent(size_t n);
    void setFrameBudget(float milliseconds);
    void start(gameres::ResourceLoader::ProgressCallback onProgress,
               gameres::ResourceLoader::CompleteCallback onComplete);

    // 每帧调用一次：在时间预算内执行主线程工作（收尾回调、启动排队中的任务）；
    // 完成数有变化时上报进度；全部结束后触发一次 onComplete。
    void tick(float dt);

    void cancel();
    float getProgress() const;
    bool isFinished() const;
    bool isCancelledFlag() const { return m_cancelled; }
    // 正常完成或被取消，ResourceLoader 据此停止 tick。
    bool isDone() const { return m_cancelled || isFinished(); }

    // 加载函数已经被调用过的资源（不限于加载成功），ResourceLoader 析构时据此延迟释放：
    // 有些资源在加载过程中就已经拿到了需要释放的东西（例如 FGUI 包的引用计数）。
    std::vector<ResourcePtr> collectHeldResources() const;

    // 已结束且失败的顶层资源；配合轮询式的加载流程使用（不依赖 onComplete 回调）。
    std::vector<ResourcePtr> getFailedResources() const;

    // 以下给 LoadTask 使用
    void completeEntry(Entry* e);
    void failEntry(Entry* e, std::string error);
    Entry* spawnChild(Entry* parent, ResourcePtr child);
    void setOnChildrenDone(Entry* parent, std::function<void()> fn);
    bool anyChildFailed(const Entry* parent) const;
    // 主线程收尾工作（onChildrenDone、runOnWorker 的 then）统一排队，在 tick 的时间预算内执行
    void postMainThreadJob(std::function<void()> job);

private:
    Entry* findOrCreateEntry(const ResourcePtr& resource, bool isRoot);
    // 在时间预算内执行主线程工作；每帧至少执行一项，保证加载一定能推进
    void runMainThreadWork();
    bool canStartEntry() const;
    void startEntry(Entry* e);
    void finishEntry(Entry* e, bool success, std::string error);
    void checkChildrenDone(Entry* parent);
    void reportProgress();

    gameres::ResourceLoaderRegistry& m_registry;

    std::vector<std::unique_ptr<Entry>> m_entries;
    std::vector<Entry*> m_roots;
    std::unordered_map<std::string, Entry*> m_byKey;  // "type#key"
    std::deque<Entry*> m_pending;
    std::deque<std::function<void()>> m_mainThreadJobs;
    size_t m_inFlight      = 0;
    size_t m_maxConcurrent = 8;
    float m_frameBudgetMs  = 8.0f;

    // 已结束的条目数（含子条目），用于判断是否需要上报进度
    size_t m_finishedEntries = 0;
    size_t m_reportedEntries = SIZE_MAX;

    gameres::ResourceLoader::ProgressCallback m_onProgress;
    gameres::ResourceLoader::CompleteCallback m_onComplete;
    bool m_started         = false;
    bool m_cancelled       = false;
    bool m_completedCalled = false;
};

}  // namespace gameres::detail
