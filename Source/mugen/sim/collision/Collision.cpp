#include "Collision.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/HitResolver.h"
#include "mugen/sim/obstacle/Obstacle.h"
#include <algorithm>

namespace mugen::sim
{
// 世界阶段回调，构造临时视图后扫描本帧接触。
void CollisionSystem::update(World& world)
{
    CollisionSystem(world).update();
}

CollisionSystem::CollisionSystem(World& world) : m_world(world) {}

// 判断双方阵营掩码是否互相允许碰撞。
bool CollisionSystem::_inHostileMasks(const RoleIdentity& first, const RoleIdentity& second)
{
    const uint32_t firstCamp = static_cast<uint32_t>(first.camp);
    const uint32_t secondCamp = static_cast<uint32_t>(second.camp);
    return (static_cast<uint32_t>(first.hostileCamp) & secondCamp) != 0 &&
           (static_cast<uint32_t>(second.hostileCamp) & firstCamp) != 0;
}

// 在屏幕平面计算两个轴对齐框的交叠中心。
bool CollisionSystem::_intersects(const WorldBox& first, const WorldBox& second, FixedVec2& point)
{
    const Fixed left   = std::max(first.pos.x, second.pos.x);
    const Fixed right  = std::min(first.pos.x + first.size.x, second.pos.x + second.size.x);
    const Fixed bottom = std::max(first.pos.z, second.pos.z);
    const Fixed top    = std::min(first.pos.z + first.size.z, second.pos.z + second.size.z);
    if (left >= right || bottom >= top)
        return false;
    point = {(left + right) * Fixed::ratio(1, 2), (bottom + top) * Fixed::ratio(1, 2)};
    return true;
}

// 查找攻击框与受击框之间的首个平面接触点。
bool CollisionSystem::_isContacting(const ColliderState& first, const ColliderState& second, FixedVec2& point)
{
    for (const auto& attack : first.attack)
        for (const auto& damage : second.damage)
            if (_intersects(attack, damage, point))
                return true;
    return false;
}

// 按实体组将接触状态变化分派给对应处理器。
void CollisionSystem::_dispatch(Entity collider, Entity other, CollisionEvent event, FixedVec2 point)
{
    const auto colliderGroup = m_world.ecs().get<EntityGroup>(collider).kind;
    const auto otherGroup    = m_world.ecs().get<EntityGroup>(other).kind;
    if (colliderGroup == GroupKind::Effect)
        hit::HitResolver(m_world, collider).onCollision(other, event, point);
    else if (colliderGroup == GroupKind::Portal || colliderGroup == GroupKind::Goods)
        room_entities::RoomEntity(m_world, collider).onCollision(other, event);
    else if ((colliderGroup == GroupKind::Role && otherGroup == GroupKind::Obstacle) ||
             (colliderGroup == GroupKind::Obstacle && otherGroup == GroupKind::Role))
        obstacle::ObstacleActor(m_world, colliderGroup == GroupKind::Obstacle ? collider : other)
            .onCollision(colliderGroup == GroupKind::Role ? collider : other, event, point);
}

// 为本帧未延续的历史接触补发结束事件。
void CollisionSystem::_endMissing(Entity collider, CollisionState& state, const std::vector<CollisionContact>& active)
{
    for (const auto& old : state.contacts)
    {
        const auto present = std::find_if(active.begin(), active.end(), [&old](const CollisionContact& contact) {
            return contact.other == old.other;
        });
        if (present == active.end() && m_world.ecs().has<EntityGroup>(old.other))
            _dispatch(collider, old.other, CollisionEvent::End, old.point);
    }
}

// 以平面框和深度半径检测接触，并维护接触状态机。
void CollisionSystem::update()
{
    auto& ecs = m_world.ecs();
    const uint64_t frame = ecs.singleton<WorldClock>().frame;
    std::vector<Entity> colliders;
    ecs.each<EntityGroup, ColliderState, RoleIdentity>([&](Entity entity, const EntityGroup&, const ColliderState&, const RoleIdentity&) {
        colliders.push_back(entity);
    });
    for (const Entity collider : colliders)
    {
        // 回调可能生成特效并扩容组件池，检测使用值快照。
        const auto source = ecs.get<ColliderState>(collider);
        if (!ecs.has<CollisionState>(collider)) ecs.add(collider, CollisionState{});
        auto state = ecs.get<CollisionState>(collider);
        std::vector<CollisionContact> active;
        if (source.registrationFrame != frame)
            for (const Entity other : colliders)
            {
                if (collider == other)
                    continue;
                const auto& target = ecs.get<ColliderState>(other);
                const auto sourceKind = ecs.get<EntityGroup>(collider).kind;
                const auto targetKind = ecs.get<EntityGroup>(other).kind;
                const bool interaction = (sourceKind == GroupKind::Portal || sourceKind == GroupKind::Goods) &&
                                         (targetKind == GroupKind::Role || targetKind == GroupKind::Obstacle);
                if (target.registrationFrame == frame || (!interaction && !_inHostileMasks(ecs.get<RoleIdentity>(collider), ecs.get<RoleIdentity>(other))))
                    continue;
                const auto& sourceTransform = ecs.get<Transform>(collider);
                const auto& targetTransform = ecs.get<Transform>(other);
                const Fixed deltaZ = sourceTransform.position.z >= targetTransform.position.z
                    ? sourceTransform.position.z - targetTransform.position.z : targetTransform.position.z - sourceTransform.position.z;
                if (deltaZ >= source.radius + target.radius)
                    continue;
                FixedVec2 point;
                bool touching = _isContacting(source, target, point);
                if (!touching && (interaction || (sourceKind == GroupKind::Role && targetKind == GroupKind::Obstacle)))
                    for (const auto& a : source.damage)
                        for (const auto& b : target.damage)
                            touching |= _intersects(a, b, point);
                if (!touching)
                    continue;
                active.push_back({other, point});
                const auto old = std::find_if(state.contacts.begin(), state.contacts.end(), [other](const CollisionContact& contact) {
                    return contact.other == other;
                });
                _dispatch(collider, other, old == state.contacts.end() ? CollisionEvent::Began : CollisionEvent::Continue, point);
            }
        _endMissing(collider, state, active);
        ecs.get<CollisionState>(collider).contacts = std::move(active);
    }
}
}  // namespace mugen::sim
