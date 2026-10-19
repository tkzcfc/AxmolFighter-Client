#include "Displacement.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/brain/RoleBrainDriver.h"
#include "mugen/sim/role/brain/RoleContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/core/math/FixedMath.h"
#include <algorithm>

namespace mugen::sim::displacement
{
// 返回定点数绝对值。
namespace
{
Fixed abs(Fixed value)
{
    return value < Fixed{} ? -value : value;
}
}  // namespace
// 积分单轴速度，保留速度和加速度的时间约束。
AxisStep integrate(Fixed time,
                   Fixed velocity,
                   Fixed acceleration,
                   Fixed accelerationTime,
                   Fixed velocityTime,
                   Fixed dt,
                   Fixed gravity)
{
    Fixed active;
    if (accelerationTime > Fixed{})
        active = time <= accelerationTime ? dt : std::max(Fixed{}, dt - (time - accelerationTime));
    else if (accelerationTime == Fixed::fromInt(-1))
        active = dt;
    Fixed next = velocity + acceleration * active + gravity * dt;
    Fixed offset;
    const auto trapezoid = [&](Fixed span) { return (velocity + next) * span / Fixed::fromInt(2); };
    if (velocityTime > Fixed{})
    {
        if (time <= velocityTime)
            offset = trapezoid(active) + next * (dt - active);
        else
        {
            const Fixed t = time - velocityTime;
            if (t < dt)
                offset = t > active ? trapezoid(active) + next * (t - active) : trapezoid(t);
            else
                next = {};
        }
    }
    else if (velocityTime == Fixed::fromInt(-1))
        offset = trapezoid(active) + next * (dt - active);
    else
        offset = next * dt;
    return {offset, next};
}

// 构造位移视图，只保存世界与实体句柄。
DisplacementDriver::DisplacementDriver(World& world, Entity entity) : m_world(world), m_entity(entity) {}

// 重置位移状态及受击施加的强制速度。
void DisplacementDriver::reset()
{
    auto& d        = m_world.ecs().get<Displacement>(m_entity);
    d.elapsedMs    = {};
    d.velocityTime = d.acceleration = d.accelerationTime    = {};
    d.gravity                                               = Fixed::fromInt(1);
    d.bounces                                               = {};
    d.listenerNode                                          = -1;
    m_world.ecs().get<RoleHitState>(m_entity).forceVelocity = {};
}

// 选择前方扇形内最近的敌对存活角色。
Entity DisplacementDriver::findTargetInSector(Fixed radius, Fixed angle)
{
    const auto origin    = m_world.ecs().get<Transform>(m_entity);
    const auto identity  = m_world.ecs().get<RoleIdentity>(m_entity);
    const auto campGroup = [](int32_t camp) { return camp == 2 || camp == 4 ? 2 : camp == 8 || camp == 16 ? 3 : 1; };
    Entity result;
    Fixed best = radius;
    m_world.ecs().each<RoleIdentity, Transform, Vitals, EntityGroup>([&](Entity candidate, const RoleIdentity& role,
                                                                         const Transform& t, const Vitals& vitals,
                                                                         const EntityGroup& group) {
        if (group.kind != GroupKind::Role || role.camp == identity.camp ||
            campGroup(role.camp) == campGroup(identity.camp) || (vitals.hp <= Fixed{} && vitals.maxHp > Fixed{}))
            return;
        const Fixed x = (t.position.x - origin.position.x) * Fixed::fromInt(origin.towardX);
        const Fixed z = t.position.z - origin.position.z;
        if (x < Fixed{})
            return;
        const Fixed distance = FixedMath::sqrt(x * x + z * z);
        if (distance > radius || (result.value != 0 && distance >= best) ||
            abs(FixedMath::atan2Deg(z, x)) > angle / Fixed::fromInt(2))
            return;
        best   = distance;
        result = candidate;
    });
    return result;
}

// 解析追踪或三轴配置并初始化位移参数。
void DisplacementDriver::setData(const conf::DisplacementConfig& data, int32_t listenerNode)
{
    auto& d        = m_world.ecs().get<Displacement>(m_entity);
    auto& velocity = m_world.ecs().get<RoleHitState>(m_entity).forceVelocity;
    const auto& t  = m_world.ecs().get<Transform>(m_entity);
    bool traced    = false;
    if (data.isTrace > 0)
    {
        const Entity target = findTargetInSector(Fixed::fromInt(data.traceRadius), Fixed::fromInt(data.traceAngle));
        if (target.value != 0)
        {
            const auto& end = m_world.ecs().get<Transform>(target).position;
            const Fixed x = end.x - t.position.x, z = end.z - t.position.z;
            const Fixed totalTime = FixedMath::sqrt(x * x + z * z) / data.traceVelocity;
            if (totalTime > Fixed{})
            {
                velocity       = {x / totalTime, {}, z / totalTime / conf::kZRate};
                d.velocityTime = {totalTime, totalTime, totalTime};
                traced         = true;
            }
        }
    }
    if (!traced)
    {
        const auto axis = [](const std::vector<Fixed>& values, size_t i) {
            return i < values.size() ? values[i] : Fixed{};
        };
        velocity = {t.vectorX * axis(data.velocity, 0), axis(data.velocity, 1), t.vectorZ * axis(data.velocity, 2)};
        d.velocityTime     = {axis(data.velocityTime, 0), axis(data.velocityTime, 1), axis(data.velocityTime, 2)};
        d.acceleration     = {axis(data.acceleration, 0), axis(data.acceleration, 1), axis(data.acceleration, 2)};
        d.accelerationTime = {axis(data.accelerationTime, 0), axis(data.accelerationTime, 1),
                              axis(data.accelerationTime, 2)};
    }
    d.gravity      = data.gravity;
    d.bounces      = data.bounces;
    d.elapsedMs    = {};
    d.listenerNode = listenerNode;
}

// 查表设置位移；-1 仅更新事件监听节点。
bool DisplacementDriver::set(int32_t id, int32_t listenerNode)
{
    if (id == -1)
    {
        m_world.ecs().get<Displacement>(m_entity).listenerNode = listenerNode;
        return true;
    }
    const auto* row = conf::Config::instance().find<conf::DisplacementConfig>(id);
    if (!row)
        return false;
    setData(*row, listenerNode);
    return true;
}

// 推进三轴位移、触发事件并限制在地图可移动区域内。
void DisplacementDriver::update()
{
    auto& ecs             = m_world.ecs();
    auto& d               = ecs.get<Displacement>(m_entity);
    auto& t               = ecs.get<Transform>(m_entity);
    auto& velocity        = ecs.get<RoleHitState>(m_entity).forceVelocity;
    const auto last       = t.position;
    const auto& role      = ecs.get<RoleStatus>(m_entity);
    const bool moving     = role.isMove() && !role.isAttack();
    const bool cannotMove = !ecs.get<ExtraState>(m_entity).isCanMove();
    auto v                = velocity;
    v.x *= moving || cannotMove ? d.velocityScale.x : d.defaultVelocityScale;
    v.y *= moving ? d.velocityScale.y : d.defaultVelocityScale;
    v.z *= moving || cannotMove ? d.velocityScale.z : d.defaultVelocityScale;
    if (cannotMove && v.y > Fixed{})
        v.y = {};
    const Fixed dt = ecs.singleton<WorldClock>().dtMs * d.extraTimeScale;
    d.elapsedMs += dt;
    const auto x =
        integrate(d.elapsedMs, v.x, t.vectorX * d.acceleration.x * (cannotMove ? d.velocityScale.x : Fixed::fromInt(1)),
                  d.accelerationTime.x, d.velocityTime.x, dt);
    const bool grounded = t.position.y == Fixed{} && v.y == Fixed{} && d.acceleration.y == Fixed{};
    const auto y        = integrate(d.elapsedMs, v.y, d.acceleration.y, d.accelerationTime.y, d.velocityTime.y, dt,
                             grounded ? Fixed{} : d.gravity * conf::kGravity);
    const auto z =
        integrate(d.elapsedMs, v.z, t.vectorZ * d.acceleration.z * (cannotMove ? d.velocityScale.z : Fixed::fromInt(1)),
                  d.accelerationTime.z, d.velocityTime.z, dt);
    velocity          = {x.velocity, y.velocity, z.velocity};
    d.offset          = {x.offset, y.offset, z.offset * conf::kZRate};
    const auto offset = d.offset;
    const auto event  = [&](DisplacementEvent e) {
        if (ecs.has<ObstacleState>(m_entity))
            return room_entities::RoomEntity(m_world, m_entity).onDisplacementEvent(e);
        return ecs.has<EffectTree>(m_entity) ? effect::EffectActor(m_world, m_entity).onDisplacementEvent(e)
                                              : role::brain::RoleContext(m_world, m_entity).onDisplacementEvent(e);
    };
    if (velocity.x == Fixed{} && velocity.y == Fixed{} && velocity.z == Fixed{})
        event(DisplacementEvent::Brake);
    t.position.x += offset.x;
    t.position.y += offset.y;
    t.position.z += offset.z;
    if (t.position.y > Fixed{})
    {
        if (y.velocity < Fixed{})
            event(DisplacementEvent::Air);
    }
    else if (t.position.y < Fixed{})
    {
        t.position.y = {};
        if (y.velocity < Fixed{})
        {
            if (y.velocity < -conf::kBounces && d.bounces > Fixed{})
            {
                if (!event(DisplacementEvent::Floor))
                    velocity.y = -y.velocity * d.bounces;
                else
                    velocity = {};
            }
            else
            {
                event(DisplacementEvent::Lie);
                velocity = {};
            }
        }
    }
    else if (y.velocity == Fixed{})
        event(DisplacementEvent::Lie);
    const Fixed radius = ecs.get<ColliderState>(m_entity).radius;
    const auto sign    = [](Fixed value) { return Fixed::fromInt(value > Fixed{} ? 1 : value < Fixed{} ? -1 : 0); };
    const FixedVec2 point{t.position.x + sign(offset.x) * radius, t.position.z + sign(offset.z) * radius};
    const auto& map    = ecs.singleton<MapRegion>();
    const auto regionX = [&] {
        if (offset.x < Fixed{})
        {
            const Fixed bound = map.getLeftRegion(point);
            if (point.x != bound && !event(DisplacementEvent::Left))
                t.position.x = bound + radius;
        }
        else if (offset.x > Fixed{})
        {
            const Fixed bound = map.getRightRegion(point);
            if (point.x != bound && !event(DisplacementEvent::Right))
                t.position.x = bound - radius;
        }
    };
    const auto regionZ = [&] {
        if (offset.z < Fixed{})
        {
            const Fixed bound = map.getBottomRegion(point);
            if (point.y != bound && !event(DisplacementEvent::Bottom))
                t.position.z = bound + radius;
        }
        else if (offset.z > Fixed{})
        {
            const Fixed bound = map.getTopRegion(point);
            if (point.y != bound && !event(DisplacementEvent::Top))
                t.position.z = bound - radius;
        }
    };
    if (!map.moveRanges.empty())
    {
        if (abs(offset.z) < abs(offset.x))
        {
            regionX();
            regionZ();
        }
        else if (abs(offset.z) > abs(offset.x))
        {
            regionZ();
            regionX();
        }
    }
    d.offset = {t.position.x - last.x, t.position.y - last.y, t.position.z - last.z};
}
}  // namespace mugen::sim::displacement
