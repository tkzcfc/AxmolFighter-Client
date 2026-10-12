#include "EntityStore.h"

#include <algorithm>
#include <limits>

namespace mugen::ecs_detail
{
// 分配槽位并按创建顺序登记新实体。
Entity EntityStore::create()
{
    MG_ASSERT(m_nextSequence < std::numeric_limits<uint64_t>::max() && "Entity sequence exhausted");
    uint32_t index;
    if (m_free.empty())
    {
        MG_ASSERT(m_slots.size() <= Entity::kIndexMask && "Entity slots exhausted");
        index = static_cast<uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }
    else
    {
        index = m_free.front();
        m_free.pop_front();
    }
    auto& slot    = m_slots[index];
    slot.alive    = true;
    slot.sequence = m_nextSequence++;
    const Entity entity{(slot.generation << Entity::kIndexBits) | index};
    m_order.push_back(entity);
    return entity;
}

// 校验句柄不是零且精确匹配当前槽位代数。
bool EntityStore::alive(Entity entity) const
{
    return entity.value != 0 && entity.index() < m_slots.size() && m_slots[entity.index()].alive &&
           m_slots[entity.index()].generation == entity.generation();
}

// 将存活槽位索引转换回当前代数的实体句柄。
Entity EntityStore::at(uint32_t index) const
{
    if (index >= m_slots.size() || !m_slots[index].alive)
        return {};
    return Entity{(m_slots[index].generation << Entity::kIndexBits) | index};
}

// 仅标记销毁，以保持当前遍历和组件访问稳定。
void EntityStore::destroy(Entity entity)
{
    if (!alive(entity) || m_slots[entity.index()].destructionOrder != 0)
        return;
    m_pending.push_back(entity);
    m_slots[entity.index()].destructionOrder = static_cast<uint32_t>(m_pending.size());
}

// 按请求顺序回收实体，耗尽代数的槽位不再复用。
void EntityStore::flushDestroyed()
{
    for (Entity entity : m_pending)
    {
        auto& slot = m_slots[entity.index()];
        slot.alive = false;
        slot.signature.reset();
        slot.destructionOrder = 0;
        // 代数耗尽时永久退役，避免陈旧句柄别名新实体。
        if (slot.generation < Entity::kMaxGeneration)
        {
            ++slot.generation;
            m_free.push_back(entity.index());
        }
    }
    std::erase_if(m_order, [this](Entity entity) { return !alive(entity); });
    m_pending.clear();
}

// 写入恢复实体生命周期所需的全部确定性状态。
void EntityStore::serialize(BinaryWriter& writer) const
{
    writer.write(static_cast<uint32_t>(m_slots.size()));
    for (const auto& slot : m_slots)
    {
        writer.write(slot.generation);
        writer.write(slot.alive);
        writer.write(slot.sequence);
        writer.write(slot.destructionOrder);
        writer.write(static_cast<uint64_t>(slot.signature.to_ullong()));
    }
    writer.write(m_nextSequence);
    writer.write(m_order);
    writer.write(static_cast<uint32_t>(m_free.size()));
    for (uint32_t index : m_free)
        writer.write(index);
}

// 读取实体状态；最终由 validate 恢复待销毁队列并检查不变量。
bool EntityStore::deserialize(BinaryReader& reader)
{
    uint32_t count = 0;
    if (!reader.read(count) || count > Entity::kIndexMask + 1)
        return false;
    for (uint32_t index = 0; index < count; ++index)
    {
        Slot slot;
        uint64_t signature = 0;
        if (!reader.read(slot.generation) || !reader.read(slot.alive) || !reader.read(slot.sequence) ||
            !reader.read(slot.destructionOrder) || !reader.read(signature))
            return false;
        slot.signature = signature;
        m_slots.push_back(slot);
    }
    if (!reader.read(m_nextSequence) || !reader.read(m_order) || !reader.read(count) || count > m_slots.size())
        return false;
    for (uint32_t offset = 0; offset < count; ++offset)
    {
        uint32_t index = 0;
        if (!reader.read(index))
            return false;
        m_free.push_back(index);
    }
    return validate();
}

// 检查快照中的顺序、签名、空闲和待销毁关系。
bool EntityStore::validate()
{
    if (m_nextSequence == 0)
        return false;
    std::vector<uint8_t> seen(m_slots.size());
    uint64_t previous = 0;
    for (Entity entity : m_order)
    {
        if (!alive(entity) || seen[entity.index()] != 0)
            return false;
        seen[entity.index()] = 1;
        const auto& slot     = m_slots[entity.index()];
        if (slot.sequence <= previous)
            return false;
        previous = slot.sequence;
        if (slot.destructionOrder != 0)
            m_pending.push_back(entity);
    }
    for (uint32_t index : m_free)
    {
        if (index >= m_slots.size() || seen[index] != 0 || m_slots[index].alive)
            return false;
        seen[index] = 1;
    }
    for (std::size_t index = 0; index < m_slots.size(); ++index)
    {
        const auto& slot = m_slots[index];
        if (slot.generation == 0 || slot.generation > Entity::kMaxGeneration || slot.sequence == 0 ||
            slot.sequence >= m_nextSequence)
            return false;
        if (!slot.alive && (slot.signature.any() || slot.destructionOrder != 0))
            return false;
        if (seen[index] == 0 && (slot.alive || slot.generation != Entity::kMaxGeneration))
            return false;
    }
    std::sort(m_pending.begin(), m_pending.end(), [this](Entity lhs, Entity rhs) {
        return m_slots[lhs.index()].destructionOrder < m_slots[rhs.index()].destructionOrder;
    });
    for (std::size_t index = 0; index < m_pending.size(); ++index)
        if (m_slots[m_pending[index].index()].destructionOrder != index + 1)
            return false;
    return true;
}
}  // namespace mugen::ecs_detail
