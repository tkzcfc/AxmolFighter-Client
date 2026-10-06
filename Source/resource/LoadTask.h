#pragma once

#include "resource/Resource.h"

#include "base/Macros.h"

#include <functional>
#include <memory>
#include <string_view>

namespace gameres
{

namespace detail
{
struct Entry;
class LoaderCore;
}  // namespace detail

// 交给已注册加载函数（见 ResourceLoaderRegistry）的任务句柄，只能在主线程使用。
// 加载函数必须恰好调用一次 complete() 或 fail()：要么直接调用，要么在
// spawn() + onChildrenDone() 的回调里调用。
//
// 所属的 ResourceLoader 被取消或销毁后，本对象的所有方法都会变成空操作，
// 不需要在回调里额外判空。
class LoadTask
{
public:
    // 恰好调用一次；重复调用会被忽略。
    void complete();
    void fail(std::string_view error);

    bool isCancelled() const;

    // 在 JobSystem 的工作线程执行 work，完成后在主线程执行 then（受 ResourceLoader 的每帧时间预算
    // 约束，可能延后到之后的帧）；任务已取消时跳过 then。
    // 注意：worker 线程中不要用相对路径调用 FileUtils（它对相对路径的缓存不是线程安全的），
    // 请先在主线程转换为全路径。
    void runOnWorker(std::function<void()> work, std::function<void()> then);

    // 派生子资源：按“类型 + getKey()”在同一个 ResourceLoader 内去重，并受同一个并发上限调度。
    // 子资源不计入总权重，它们的完成情况均分计入当前任务的进度，不会让进度条倒退。
    template <class T, class... Args>
    std::shared_ptr<T> spawn(Args&&... args)
    {
        auto result = spawnResource(std::make_shared<T>(std::forward<Args>(args)...));
        AXASSERT(dynamic_cast<T*>(result.get()), "LoadTask::spawn: resource type/key already used by another class");
        return std::static_pointer_cast<T>(result);
    }

    // 所有已 spawn 的子资源都结束（成功或失败）后，在主线程调用一次 fn（受每帧时间预算约束，
    // 在之后某次 tick 中执行），由 fn 负责收尾并调用 complete()/fail()。
    // 必须在所有 spawn() 之后、加载函数返回之前注册；没有 spawn 过子资源时在下一次处理时执行。
    void onChildrenDone(std::function<void()> fn);

    // 是否有子资源失败；在 onChildrenDone 的回调里用来决定走 complete() 还是 fail()。
    bool anyChildFailed() const;

private:
    friend class detail::LoaderCore;
    LoadTask(std::weak_ptr<detail::LoaderCore> core, detail::Entry* entry) : m_core(std::move(core)), m_entry(entry) {}

    ResourcePtr spawnResource(ResourcePtr child);

    std::weak_ptr<detail::LoaderCore> m_core;
    detail::Entry* m_entry;
};

using LoadTaskPtr = std::shared_ptr<LoadTask>;

}  // namespace gameres
