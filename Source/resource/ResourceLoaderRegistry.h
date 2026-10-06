#pragma once

#include "resource/LoadTask.h"
#include "resource/Resource.h"
#include "resource/ResourceType.h"

#include <functional>
#include <memory>
#include <unordered_map>

namespace gameres
{

// 注册表：记录每种 ResourceType 对应的加载函数和进度权重。
// getInstance() 返回的全局单例在构造时自动注册内置类型（见 builtin/BuiltinLoaders.h）。
// 开发者可以调用 registerType<T>() 注册自定义类型，也可以用同样的方式覆盖内置实现。
// 测试可以直接构造一个空实例，只注册测试用的加载函数。
class ResourceLoaderRegistry
{
public:
    using LoaderFn = std::function<void(const ResourcePtr&, const LoadTaskPtr&)>;

    ResourceLoaderRegistry() = default;

    static ResourceLoaderRegistry& getInstance();

    // fn 在资源被调度执行时于主线程调用一次，异步流程通过 task 完成（见 LoadTask.h）。
    // weight 是该类型在总进度中的占比权重：越复杂、越耗时的类型可以设得越大，
    // 避免进度条长时间卡在接近 100% 的位置。
    template <class T>
    void registerType(ResourceType type,
                      std::function<void(std::shared_ptr<T>, const LoadTaskPtr&)> fn,
                      float weight = 1.0f)
    {
        registerRaw(type, [fn = std::move(fn)](const ResourcePtr& res, const LoadTaskPtr& task) {
            fn(std::static_pointer_cast<T>(res), task);
        }, weight);
    }

    void setWeight(ResourceType type, float weight);
    // 未注册的类型返回 1；负数按 0 处理。
    float getWeight(ResourceType type) const;

    // 返回空指针表示该类型没有注册加载函数。
    const LoaderFn* find(ResourceType type) const;

private:
    void registerRaw(ResourceType type, LoaderFn fn, float weight);

    struct Entry
    {
        LoaderFn fn;
        float weight = 1.0f;
    };

    std::unordered_map<ResourceType, Entry> m_entries;
};

}  // namespace gameres
