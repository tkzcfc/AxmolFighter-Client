#pragma once

#include "ComponentPool.h"
#include "../serialize/Hash.h"

#include <tuple>

namespace mugen
{
namespace ecs_detail
{
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

class TraversalScope
{
public:
    explicit TraversalScope(uint32_t& depth) : m_depth(depth) { ++m_depth; }
    ~TraversalScope() { --m_depth; }
    TraversalScope(const TraversalScope&)            = delete;
    TraversalScope& operator=(const TraversalScope&) = delete;

private:
    uint32_t& m_depth;
};
}  // namespace ecs_detail

// Registry tuple order comes from the owning layer's X-macro lists and fixes serialized type IDs.
template <class Components = std::tuple<>, class Singletons = std::tuple<>>
class ECSManager;

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

    Entity create() { return m_entities.create(); }
    void destroy(Entity entity) { m_entities.destroy(entity); }
    bool alive(Entity entity) const { return m_entities.alive(entity); }

    void flushDestroyed()
    {
        MG_ASSERT(m_traversalDepth == 0 && "Cannot flush during traversal");
        if (m_traversalDepth != 0)
            return;
        for (Entity entity : m_entities.pending())
            std::apply([entity](auto&... pools) { (pools.remove(entity), ...); }, m_pools);
        m_entities.flushDestroyed();
    }

    template <class T>
    T& add(Entity entity, T value)
    {
        MG_ASSERT(alive(entity) && "Cannot add a component to an invalid entity");
        auto& result = std::get<componentId<T>>(m_pools).add(entity, std::move(value));
        m_entities.setComponent(entity, componentId<T>, true);
        return result;
    }

    template <class T>
    void remove(Entity entity)
    {
        if (!alive(entity))
            return;
        std::get<componentId<T>>(m_pools).remove(entity);
        m_entities.setComponent(entity, componentId<T>, false);
    }

    template <class T>
    T* tryGet(Entity entity)
    {
        return const_cast<T*>(std::as_const(*this).template tryGet<T>(entity));
    }

    template <class T>
    const T* tryGet(Entity entity) const
    {
        return alive(entity) ? std::get<componentId<T>>(m_pools).tryGet(entity) : nullptr;
    }

    template <class T>
    T& get(Entity entity)
    {
        return const_cast<T&>(std::as_const(*this).template get<T>(entity));
    }

    template <class T>
    const T& get(Entity entity) const
    {
        const T* value = tryGet<T>(entity);
        MG_ASSERT(value != nullptr && "Required component is missing");
        return *value;
    }

    template <class T>
    bool has(Entity entity) const
    {
        return tryGet<T>(entity) != nullptr;
    }

    template <class T>
    T& singleton()
    {
        return std::get<singletonId<T>>(m_singletons);
    }
    template <class T>
    const T& singleton() const
    {
        return std::get<singletonId<T>>(m_singletons);
    }

    // Callback: fn(Entity, Ts&...). Adding/removing a pool element invalidates its references;
    // reacquire with get/tryGet after structural changes. Flush is only allowed outside each.
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

    void serialize(ByteBuffer& buffer) const
    {
        BinaryWriter writer(buffer);
        m_entities.serialize(writer);
        std::apply([&writer](const auto&... pools) { (pools.serialize(writer), ...); }, m_pools);
        std::apply([&writer](const auto&... values) { (writer.write(values), ...); }, m_singletons);
        writer.finish();
    }

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

    ECSManager clone() const
    {
        ECSManager result       = *this;
        result.m_traversalDepth = 0;
        return result;
    }

    uint64_t hash() const
    {
        ByteBuffer buffer;
        serialize(buffer);
        return xxh64(buffer.data(), buffer.len());
    }

private:
    ecs_detail::EntityStore m_entities;
    std::tuple<ComponentPool<Components>...> m_pools;
    std::tuple<Singletons...> m_singletons;
    uint32_t m_traversalDepth = 0;
};
}  // namespace mugen
