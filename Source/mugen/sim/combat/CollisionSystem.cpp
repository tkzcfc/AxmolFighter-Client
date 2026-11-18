// [7.1] CollisionSystem
// 读：`HitboxesComponent`、`IdentityComponent`(阵营)、`ContactMemoryComponent`
// 写：`ContactMemoryComponent`
// 消费：—
// 产生：`ContactEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/anim/HitboxRules.h"
#include <algorithm>

namespace mugen::sim
{
namespace
{
// 双方阵营掩码是否互相允许碰撞。
bool inHostileMasks(const IdentityComponent& first, const IdentityComponent& second)
{
    return (static_cast<uint32_t>(first.hostileMask) & static_cast<uint32_t>(second.camp)) != 0 &&
           (static_cast<uint32_t>(second.hostileMask) & static_cast<uint32_t>(first.camp)) != 0;
}
// 攻击框 × 受击框的首个三维接触点（x 横向、y 高度、z 纵深）。
bool isContacting(const HitboxesComponent& first, const HitboxesComponent& second, FixedVec3& point)
{
    for (const auto& attack : first.attack)
        for (const auto& damage : second.damage)
            if (worldBoxesOverlap(attack, damage, &point))
                return true;
    return false;
}
}  // namespace

// 系统 CollisionSystem：7.1 行，见 DESIGN.md §3.2。
void runCollision(World& world)
{
    const uint64_t frame = world.frame();
    std::vector<Entity> colliders;
    world.ecs().each<IdentityComponent, HitboxesComponent>([&](Entity entity, const IdentityComponent&, const HitboxesComponent&) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        colliders.push_back(entity);
    });
    for (const Entity collider : colliders)
    {
        const auto& source      = world.ecs().get<HitboxesComponent>(collider);
        const auto& sourceIdent = world.ecs().get<IdentityComponent>(collider);
        if (!world.ecs().has<ContactMemoryComponent>(collider))
            world.ecs().add(collider, ContactMemoryComponent{});
        auto& memory = world.ecs().get<ContactMemoryComponent>(collider);
        std::vector<Entity> active;
        for (const Entity other : colliders)
        {
            if (collider == other)
                continue;
            const auto& target      = world.ecs().get<HitboxesComponent>(other);
            const auto& targetIdent = world.ecs().get<IdentityComponent>(other);
            // 出生帧内实体不参与碰撞（R3）。
            if (sourceIdent.born.frame == frame || targetIdent.born.frame == frame)
                continue;
            if (!inHostileMasks(sourceIdent, targetIdent))
                continue;
            FixedVec3 point;
            if (!isContacting(source, target, point))
                continue;
            active.push_back(other);
            const bool began = std::find(memory.contacts.begin(), memory.contacts.end(), other) ==
                               memory.contacts.end();
            ContactEvent event;
            event.a     = collider;
            event.b     = other;
            event.phase = began ? ContactPhase::Began : ContactPhase::Continue;
            event.point = point;
            world.ecs().singleton<EventQueue<ContactEvent>>().push(event);
        }
        for (const Entity old : memory.contacts)
        {
            if (std::find(active.begin(), active.end(), old) != active.end())
                continue;
            ContactEvent event;
            event.a     = collider;
            event.b     = old;
            event.phase = ContactPhase::End;
            world.ecs().singleton<EventQueue<ContactEvent>>().push(event);
        }
        memory.contacts = std::move(active);
    }
    // R5：接触按（攻击实体创建序，目标创建序）排序，与 CombatSystem 微管线一致。
    std::vector<Entity> created;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent&) { created.push_back(entity); });
    auto rank = [&](Entity entity) {
        const auto it = std::find(created.begin(), created.end(), entity);
        return it == created.end() ? created.size() : static_cast<size_t>(it - created.begin());
    };
    auto& queue = world.ecs().singleton<EventQueue<ContactEvent>>().items;
    std::stable_sort(queue.begin(), queue.end(), [&](const ContactEvent& a, const ContactEvent& b) {
        const size_t attacker = rank(a.a);
        const size_t other    = rank(b.a);
        if (attacker != other)
            return attacker < other;
        return rank(a.b) < rank(b.b);
    });
}
}  // namespace mugen::sim
