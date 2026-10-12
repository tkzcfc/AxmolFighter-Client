#pragma once

#include "ComponentPool.h"
#include "../serialize/Hash.h"

#include <tuple>

namespace mugen
{
namespace ecs_detail
{
// 在固定注册元组中计算唯一的序列化类型编号。
template <class T, class... Ts>
consteval std::size_t typeId()
{
    static_assert((std::size_t{0} + ... + std::size_t{std::is_same_v<T, Ts>}) == 1,
                  "Type must occur exactly once in its ECS registry");
    constexpr std::array matches{std::is_same_v<T, Ts>...};
    for (std::size_t index = 0; index < matches.size(); ++index)
        if (matches[index])
            return index;
    return 0;
}

// 用 RAII 追踪 each 遍历深度，禁止遍历中回收实体。
class TraversalScope
{
public:
    // 进入遍历作用域并增加深度。
    explicit TraversalScope(uint32_t& depth) : m_depth(depth) { ++m_depth; }
    // 离开遍历作用域并恢复深度。
    ~TraversalScope() { --m_depth; }
    TraversalScope(const TraversalScope&)            = delete;
    TraversalScope& operator=(const TraversalScope&) = delete;

private:
    // 由 ECSManager 持有的遍历深度计数器。
    uint32_t& m_depth;
};
}  // namespace ecs_detail

// 注册元组顺序来自所属层的 X 宏列表，并固定序列化类型编号。
template <class Components = std::tuple<>, class Singletons = std::tuple<>>
class ECSManager;

// 管理实体组件池、单例和可确定性快照的值类型 ECS。
template <class... Components, class... Singletons>
class ECSManager<std::tuple<Components...>, std::tuple<Singletons...>>
{
    static_assert(sizeof...(Components) <= 64, "Component signature supports at most 64 types");
    static_assert(((reflect::isReflected<Singletons> && std::is_copy_constructible_v<Singletons> &&
                    std::is_copy_assignable_v<Singletons> && !std::is_polymorphic_v<Singletons>) &&
                   ...),
                  "Singletons must be copyable reflected data without virtual functions");

public:
    template <class T>
    static constexpr std::size_t componentId = ecs_detail::typeId<T, Components...>();
    template <class T>
    static constexpr std::size_t singletonId = ecs_detail::typeId<T, Singletons...>();

    // 创建实体；组件必须随后单独添加。
    Entity create() { return m_entities.create(); }
    // 标记实体销毁，实际回收延后到 flushDestroyed。
    void destroy(Entity entity) { m_entities.destroy(entity); }
    // 检查实体句柄当前是否存活。
    bool alive(Entity entity) const { return m_entities.alive(entity); }

    // 删除待销毁实体的全部组件并回收实体；遍历中禁止调用。
    void flushDestroyed()
    {
        MG_ASSERT(m_traversalDepth == 0 && "Cannot flush during traversal");
        if (m_traversalDepth != 0)
            return;
        for (Entity entity : m_entities.pending())
            std::apply([entity](auto&... pools) { (pools.remove(entity), ...); }, m_pools);
        m_entities.flushDestroyed();
    }

    // 为存活实体添加指定组件并更新签名。
    template <class T>
    T& add(Entity entity, T value)
    {
        MG_ASSERT(alive(entity) && "Cannot add a component to an invalid entity");
        auto& result = std::get<componentId<T>>(m_pools).add(entity, std::move(value));
        m_entities.setComponent(entity, componentId<T>, true);
        return result;
    }

    // 移除实体指定组件；无效实体不执行操作。
    template <class T>
    void remove(Entity entity)
    {
        if (!alive(entity))
            return;
        std::get<componentId<T>>(m_pools).remove(entity);
        m_entities.setComponent(entity, componentId<T>, false);
    }

    // 查询可变组件；缺失或无效实体返回空。
    template <class T>
    T* tryGet(Entity entity)
    {
        return const_cast<T*>(std::as_const(*this).template tryGet<T>(entity));
    }

    // 查询只读组件；缺失或无效实体返回空。
    template <class T>
    const T* tryGet(Entity entity) const
    {
        return alive(entity) ? std::get<componentId<T>>(m_pools).tryGet(entity) : nullptr;
    }

    // 取得必需组件；缺失时触发断言。
    template <class T>
    T& get(Entity entity)
    {
        return const_cast<T&>(std::as_const(*this).template get<T>(entity));
    }

    // 取得必需只读组件；缺失时触发断言。
    template <class T>
    const T& get(Entity entity) const
    {
        const T* value = tryGet<T>(entity);
        MG_ASSERT(value != nullptr && "Required component is missing");
        return *value;
    }

    // 判断实体是否拥有指定组件。
    template <class T>
    bool has(Entity entity) const
    {
        return tryGet<T>(entity) != nullptr;
    }

    // 返回按注册表编号保存的可变单例。
    template <class T>
    T& singleton()
    {
        return std::get<singletonId<T>>(m_singletons);
    }
    // 返回按注册表编号保存的只读单例。
    template <class T>
    const T& singleton() const
    {
        return std::get<singletonId<T>>(m_singletons);
    }

    // 回调形如 fn(Entity, Ts&...)；组件增删会使引用失效，结构变化后必须重新查询，遍历内禁止回收。
    template <class... Ts, class Fn>
    void each(Fn&& fn)
    {
        ecs_detail::TraversalScope scope(m_traversalDepth);
        const std::size_t end = m_entities.order().size();
        std::bitset<64> required;
        (required.set(componentId<Ts>), ...);
        for (std::size_t index = 0; index < end; ++index)
        {
            const Entity entity = m_entities.order()[index];
            if ((m_entities.signature(entity) & required) == required)
                fn(entity, *std::get<componentId<Ts>>(m_pools).tryGet(entity)...);
        }
    }

    // 写入实体、组件池和单例的确定性快照。
    void serialize(ByteBuffer& buffer) const
    {
        BinaryWriter writer(buffer);
        m_entities.serialize(writer);
        std::apply([&writer](const auto&... pools) { (pools.serialize(writer), ...); }, m_pools);
        std::apply([&writer](const auto&... values) { (writer.write(values), ...); }, m_singletons);
        writer.finish();
    }

    // 仅向从未分配实体的 manager 读取完整快照，失败不修改目标。
    bool deserialize(ByteBuffer& buffer)
    {
        if (!m_entities.empty() || m_traversalDepth != 0)
            return false;
        ECSManager restored;
        BinaryReader reader(buffer);
        if (!restored.m_entities.deserialize(reader))
            return false;
        for (Entity entity : restored.m_entities.order())
            if ((restored.m_entities.signature(entity) >> sizeof...(Components)).any())
                return false;
        const bool poolsOk = (std::get<componentId<Components>>(restored.m_pools)
                                  .deserialize(reader, restored.m_entities, componentId<Components>) &&
                              ...);
        if (!poolsOk)
            return false;
        const bool singletonsOk =
            std::apply([&reader](auto&... values) { return (reader.read(values) && ...); }, restored.m_singletons);
        if (!singletonsOk)
            return false;
        *this = std::move(restored);
        return true;
    }

    // 深拷贝当前 ECS；副本不继承遍历作用域。
    ECSManager clone() const
    {
        ECSManager result       = *this;
        result.m_traversalDepth = 0;
        return result;
    }

    // 对序列化快照计算 xxh64 哈希。
    uint64_t hash() const
    {
        ByteBuffer buffer;
        serialize(buffer);
        return xxh64(buffer.data(), buffer.len());
    }

private:
    // 保存实体生命周期与签名。
    ecs_detail::EntityStore m_entities;
    // 按组件注册顺序排列的稀疏组件池。
    std::tuple<ComponentPool<Components>...> m_pools;
    // 按单例注册顺序保存的值状态。
    std::tuple<Singletons...> m_singletons;
    // 当前嵌套 each 调用深度。
    uint32_t m_traversalDepth = 0;
};
}  // namespace mugen
