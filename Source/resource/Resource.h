#pragma once

#include "resource/ResourceType.h"

#include <memory>
#include <string>

namespace gameres
{

namespace detail
{
class LoaderCore;
}  // namespace detail

enum class ResourceState : uint8_t
{
    Pending,
    Loading,
    Loaded,
    Failed,
    Cancelled,
};

// 资源描述基类：描述“加载什么”，并持有加载完成后的结果。
// 具体“怎么加载”由 ResourceLoaderRegistry 中按类型注册的加载函数负责（见 ResourceLoaderRegistry.h）。
//
// 扩展自定义类型：
//   1. 在 ResourceType 中追加一个值（从 CustomBegin 往后编号）。
//   2. 继承 Resource，定义构造参数和加载结果的存储字段；复杂资源可以在一个子类里管理多个文件
//      （例如 Spine 需要 .skel + .atlas 列表 + scale）。
//   3. 需要时实现 releaseHold()：释放加载期间为了保活结果而持有的引用。
//   4. 调用 ResourceLoaderRegistry::getInstance().registerType<MyResource>(type, loaderFn, weight)。
//      loaderFn 签名为 void(std::shared_ptr<MyResource>, const LoadTaskPtr&)：在其中可以用
//      task->runOnWorker(...) 做耗时解码，再于主线程调用 task->complete()/fail()；复杂资源可以用
//      task->spawn<T>(...) 派生子资源（详见 LoadTask.h）。
class Resource
{
public:
    explicit Resource(ResourceType type) : m_type(type) {}
    virtual ~Resource() = default;

    Resource(const Resource&)            = delete;
    Resource& operator=(const Resource&) = delete;

    ResourceType getType() const { return m_type; }

    // 用于同一个 ResourceLoader 内去重和日志；同类型下 key 相同视为同一份资源。
    virtual std::string getKey() const = 0;

    ResourceState getState() const { return m_state; }
    const std::string& getError() const { return m_error; }
    bool isLoaded() const { return m_state == ResourceState::Loaded; }

    // 释放加载期间持有的结果（例如 RefPtr<Texture2D>、MgSkeletonDataPtr、FGUI 包引用计数）。
    // 由 ResourceLoader 在析构之后的下一帧调用，详见 ResourceLoader.h 的生命周期说明。
    // 取消或失败的资源也会被调用：从未真正拿到结果时必须安全地空操作。
    virtual void releaseHold() {}

private:
    friend class detail::LoaderCore;

    // 以下只由 LoaderCore 调用。
    void markLoading() { m_state = ResourceState::Loading; }
    void markLoaded() { m_state = ResourceState::Loaded; }
    void markFailed(std::string error)
    {
        m_state = ResourceState::Failed;
        m_error = std::move(error);
    }
    void markCancelled() { m_state = ResourceState::Cancelled; }

    ResourceType m_type;
    ResourceState m_state = ResourceState::Pending;
    std::string m_error;
};

using ResourcePtr = std::shared_ptr<Resource>;

}  // namespace gameres
