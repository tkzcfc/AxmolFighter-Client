#pragma once

#include "EntityStore.h"

#include <limits>
#include <utility>

namespace mugen
{
// 采用稀疏索引与紧凑数据数组保存单一组件类型。
template <class T>
class ComponentPool
{
    static_assert(reflect::isReflected<T> && std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> &&
                      !std::is_polymorphic_v<T>,
                  "Components must be copyable reflected data without virtual functions");

public:
    // 查询可变组件；实体不拥有该组件时返回空。
    T* tryGet(Entity entity) { return const_cast<T*>(std::as_const(*this).tryGet(entity)); }

    // 查询只读组件；同时验证稀疏映射中的完整实体句柄。
    const T* tryGet(Entity entity) const
    {
        if (entity.index() >= m_sparse.size())
            return nullptr;
        const uint32_t dense = m_sparse[entity.index()];
        if (dense == kAbsent || m_entities[dense] != entity)
            return nullptr;
        return &m_dense[dense];
    }

    // 插入新组件；调用方必须确保实体尚未拥有该类型。
    T& add(Entity entity, T value)
    {
        MG_ASSERT(tryGet(entity) == nullptr && "Component already exists");
        if (m_sparse.size() <= entity.index())
            m_sparse.resize(static_cast<std::size_t>(entity.index()) + 1, kAbsent);
        m_sparse[entity.index()] = static_cast<uint32_t>(m_dense.size());
        m_dense.push_back(std::move(value));
        m_entities.push_back(entity);
        return m_dense.back();
    }

    // 以末尾元素填补空洞，故会使组件引用失效。
    void remove(Entity entity)
    {
        if (tryGet(entity) == nullptr)
            return;
        const uint32_t dense = m_sparse[entity.index()];
        if (dense + 1 != m_dense.size())
        {
            m_dense[dense]                      = std::move(m_dense.back());
            m_entities[dense]                   = m_entities.back();
            m_sparse[m_entities[dense].index()] = dense;
        }
        m_dense.pop_back();
        m_entities.pop_back();
        m_sparse[entity.index()] = kAbsent;
    }

    // 按紧凑数组顺序写入实体索引及组件载荷。
    void serialize(BinaryWriter& writer) const
    {
        writer.write(static_cast<uint32_t>(m_dense.size()));
        for (std::size_t index = 0; index < m_dense.size(); ++index)
        {
            writer.write(m_entities[index].index());
            writer.write(m_dense[index]);
        }
    }

    // 读取组件，并与实体签名逐项交叉验证。
    bool deserialize(BinaryReader& reader, const ecs_detail::EntityStore& entities, std::size_t typeId)
    {
        uint32_t count = 0;
        if (!reader.read(count) || count > entities.order().size())
            return false;
        for (uint32_t offset = 0; offset < count; ++offset)
        {
            uint32_t index = 0;
            T value{};
            if (!reader.read(index))
                return false;
            const Entity entity = entities.at(index);
            if (entity.value == 0 || tryGet(entity) != nullptr || !entities.signature(entity).test(typeId) ||
                !reader.read(value))
                return false;
            add(entity, std::move(value));
        }
        for (Entity entity : entities.order())
            if (entities.signature(entity).test(typeId) != (tryGet(entity) != nullptr))
                return false;
        return true;
    }

private:
    // 稀疏映射中表示无组件的哨兵值。
    static constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();
    // 实体槽位索引到紧凑数组索引的映射。
    std::vector<uint32_t> m_sparse;
    // 连续存储的组件值。
    std::vector<T> m_dense;
    // 与紧凑组件值同下标的完整实体句柄。
    std::vector<Entity> m_entities;
};
}  // namespace mugen
