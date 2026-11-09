// [7.1] CollisionSystem
// 读：`Hitboxes`、`Transform`、`Identity`(阵营)、`ContactMemory`
// 写：`ContactMemory`
// 消费：—
// 产生：`ContactEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include <algorithm>

namespace mugen::sim
{
namespace
{
// 双方阵营掩码是否互相允许碰撞。
bool inHostileMasks(const Identity& first, const Identity& second)
{
    return (static_cast<uint32_t>(first.m_hostileMask) & static_cast<uint32_t>(second.m_camp)) != 0 &&
           (static_cast<uint32_t>(second.m_hostileMask) & static_cast<uint32_t>(first.m_camp)) != 0;
}
// 屏幕平面两轴对齐框交叠中心。
bool intersects(const WorldBox& first, const WorldBox& second, FixedVec2& point)
{
    const Fixed left   = std::max(first.m_pos.x, second.m_pos.x);
    const Fixed right  = std::min(first.m_pos.x + first.m_size.x, second.m_pos.x + second.m_size.x);
    const Fixed bottom = std::max(first.m_pos.z, second.m_pos.z);
    const Fixed top    = std::min(first.m_pos.z + first.m_size.z, second.m_pos.z + second.m_size.z);
    if (left >= right || bottom >= top)
        return false;
    point = {(left + right) * Fixed::ratio(1, 2), (bottom + top) * Fixed::ratio(1, 2)};
    return true;
}
// 攻击框 × 受击框的首个接触点。
bool isContacting(const Hitboxes& first, const Hitboxes& second, FixedVec2& point)
{
    for (const auto& attack : first.m_attack)
        for (const auto& damage : second.m_damage)
            if (intersects(attack, damage, point))
                return true;
    return false;
}
}  // namespace

// 系统 CollisionSystem：7.1 行，见 DESIGN.md §3.2。
void runCollision(World& world)
{
    const uint64_t frame = world.frame();
    std::vector<Entity> colliders;
    world.ecs().each<Identity, Hitboxes>([&](Entity entity, const Identity&, const Hitboxes&) {
        colliders.push_back(entity);
    });
    for (const Entity collider : colliders)
    {
        const auto& source      = world.ecs().get<Hitboxes>(collider);
        const auto& sourceIdent = world.ecs().get<Identity>(collider);
        const auto& sourceTrans = world.ecs().get<Transform>(collider);
        if (!world.ecs().has<ContactMemory>(collider))
            world.ecs().add(collider, ContactMemory{});
        auto& memory = world.ecs().get<ContactMemory>(collider);
        std::vector<Entity> active;
        for (const Entity other : colliders)
        {
            if (collider == other)
                continue;
            const auto& target      = world.ecs().get<Hitboxes>(other);
            const auto& targetIdent = world.ecs().get<Identity>(other);
            const auto& targetTrans = world.ecs().get<Transform>(other);
            // 出生帧内实体不参与碰撞（R3）。
            if (sourceIdent.m_born.m_frame == frame || targetIdent.m_born.m_frame == frame)
                continue;
            if (!inHostileMasks(sourceIdent, targetIdent))
                continue;
            const Fixed deltaZ = sourceTrans.m_position.z >= targetTrans.m_position.z
                                     ? sourceTrans.m_position.z - targetTrans.m_position.z
                                     : targetTrans.m_position.z - sourceTrans.m_position.z;
            if (deltaZ >= source.m_radius + target.m_radius)
                continue;
            FixedVec2 point;
            if (!isContacting(source, target, point))
                continue;
            active.push_back(other);
            const bool began = std::find(memory.m_contacts.begin(), memory.m_contacts.end(), other) ==
                               memory.m_contacts.end();
            ContactEvent event;
            event.m_a     = collider;
            event.m_b     = other;
            event.m_phase = began ? ContactPhase::Began : ContactPhase::Continue;
            event.m_point = {point.x, {}, point.y};
            world.ecs().singleton<EventQueue<ContactEvent>>().push(event);
        }
        for (const Entity old : memory.m_contacts)
        {
            if (std::find(active.begin(), active.end(), old) != active.end())
                continue;
            ContactEvent event;
            event.m_a     = collider;
            event.m_b     = old;
            event.m_phase = ContactPhase::End;
            world.ecs().singleton<EventQueue<ContactEvent>>().push(event);
        }
        memory.m_contacts = std::move(active);
    }
}
}  // namespace mugen::sim
