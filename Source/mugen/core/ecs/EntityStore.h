#pragma once

#include "Entity.h"
#include "../serialize/BinaryArchive.h"

#include <bitset>
#include <deque>
#include <span>

namespace mugen::ecs_detail
{
// 实体生命周期元数据；组件归属由 ECSManager 管理。
class EntityStore
{
public:
    // 分配新实体，优先复用 FIFO 空闲槽位。
    Entity create();
    // 标记实体等待批量销毁，重复请求无效。
    void destroy(Entity entity);
    // 检查句柄索引、代数与存活标记是否一致。
    bool alive(Entity entity) const;
    // 回收待销毁实体并递增可复用槽位的代数。
    void flushDestroyed();
    bool empty() const { return m_slots.empty(); }
    // 返回稳定的创建顺序。
    const std::vector<Entity>& order() const { return m_order; }
    // 返回按销毁请求顺序排列的待回收实体。
    std::span<const Entity> pending() const { return m_pending; }
    // 返回实体当前的组件存在位图。
    const std::bitset<64>& signature(Entity entity) const { return m_slots[entity.index()].signature; }
    // 更新实体某个固定类型编号的组件存在位。
    void setComponent(Entity entity, std::size_t id, bool present)
    {
        m_slots[entity.index()].signature.set(id, present);
    }
    // 按槽位索引重建存活句柄，非存活槽位返回无效句柄。
    Entity at(uint32_t index) const;
    std::size_t slotCount() const { return m_slots.size(); }
    // 按快照格式写入槽位、顺序与 FIFO 空闲队列。
    void serialize(BinaryWriter& writer) const;
    // 读取快照并验证内部生命周期不变量。
    bool deserialize(BinaryReader& reader);

private:
    // 单个实体槽位的生命周期及组件签名。
    struct Slot
    {
        // 用于拒绝陈旧句柄的当前代数。
        uint32_t generation = 1;
        // 指示槽位当前是否分配给实体。
        bool alive = false;
        // 保持遍历稳定性的单调创建序号。
        uint64_t sequence = 0;
        // 待销毁队列中的一基次序，零表示未标记。
        uint32_t destructionOrder = 0;
        // 各组件类型是否存在的固定宽度位图。
        std::bitset<64> signature;
    };

    // 验证反序列化后的槽位、顺序、待销毁和空闲队列关系。
    bool validate();

    // 所有已分配过的实体槽位。
    std::vector<Slot> m_slots;
    // 按创建次序排列的当前存活实体。
    std::vector<Entity> m_order;
    // 可复用槽位的 FIFO 队列。
    std::deque<uint32_t> m_free;
    // 延后到 flushDestroyed 回收的实体。
    std::vector<Entity> m_pending;
    // 分配给新实体的下一个非零创建序号。
    uint64_t m_nextSequence = 1;
};
}  // namespace mugen::ecs_detail
