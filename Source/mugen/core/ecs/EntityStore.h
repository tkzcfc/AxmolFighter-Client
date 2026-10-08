#pragma once

#include "Entity.h"
#include "../serialize/BinaryArchive.h"

#include <bitset>
#include <deque>
#include <span>

namespace mugen::ecs_detail
{
// Entity lifetime metadata; component ownership belongs to ECSManager.
class EntityStore
{
public:
    Entity create();
    void destroy(Entity entity);
    bool alive(Entity entity) const;
    void flushDestroyed();
    bool empty() const { return m_slots.empty(); }
    const std::vector<Entity>& order() const { return m_order; }
    std::span<const Entity> pending() const { return m_pending; }
    const std::bitset<64>& signature(Entity entity) const { return m_slots[entity.index()].signature; }
    void setComponent(Entity entity, std::size_t id, bool present)
    {
        m_slots[entity.index()].signature.set(id, present);
    }
    Entity at(uint32_t index) const;
    std::size_t slotCount() const { return m_slots.size(); }
    void serialize(BinaryWriter& writer) const;
    bool deserialize(BinaryReader& reader);

private:
    struct Slot
    {
        uint32_t generation       = 1;
        bool alive                = false;
        uint64_t sequence         = 0;
        uint32_t destructionOrder = 0;
        std::bitset<64> signature;
    };

    bool validate();

    std::vector<Slot> m_slots;
    std::vector<Entity> m_order;
    std::deque<uint32_t> m_free;
    std::vector<Entity> m_pending;
    uint64_t m_nextSequence = 1;
};
}  // namespace mugen::ecs_detail
