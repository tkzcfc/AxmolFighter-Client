#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim::displacement
{
// 积分单轴速度，保留速度和加速度的时间约束。
AxisStep integrate(Fixed time,
                   Fixed velocity,
                   Fixed acceleration,
                   Fixed accelerationTime,
                   Fixed velocityTime,
                   Fixed dt,
                   Fixed gravity)
{
    Fixed active = {};
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

namespace rules
{
namespace
{
// 读取配置数组中可省略的尾项。
Fixed axis(const std::vector<Fixed>& values, size_t index)
{
    return index < values.size() ? values[index] : Fixed{};
}
}  // namespace

// 按配置标识设置位移状态。
bool set(World& world, Entity entity, int32_t id)
{
    if (id == -1)
        return true;
    const auto* row = conf::Config::instance().find<conf::DisplacementConfig>(id);
    if (!row)
        return false;
    auto& d        = world.ecs().get<Displacement>(entity);
    auto& velocity = d.m_velocity;
    const auto& t  = world.ecs().get<Transform>(entity);
    // 上一段位移仍在进行则记录待结算的 disp-（仅摘要簿记，存于 World 非序列化表；DisplacementSystem 积分后发出，终点取帧末位置）。
    if (d.m_curveId != -1 && d.m_curveId != id)
        world.setPendingDispEnd(entity, d.m_curveId);
    // B2 不实现追踪位移（isTrace），按普通三轴配置。
    velocity            = {t.m_vectorX * axis(row->velocity, 0), axis(row->velocity, 1),
                           t.m_vectorZ * axis(row->velocity, 2)};
    d.m_velocityTime    = {axis(row->velocityTime, 0), axis(row->velocityTime, 1), axis(row->velocityTime, 2)};
    d.m_acceleration    = {axis(row->acceleration, 0), axis(row->acceleration, 1), axis(row->acceleration, 2)};
    d.m_accelerationTime = {axis(row->accelerationTime, 0), axis(row->accelerationTime, 1),
                            axis(row->accelerationTime, 2)};
    d.m_gravity         = row->gravity;
    d.m_bounces         = row->bounces;
    d.m_elapsedMs       = {};
    d.m_curveId         = id;
    return true;
}

// 重置位移状态与速度。
void reset(World& world, Entity entity)
{
    auto& d          = world.ecs().get<Displacement>(entity);
    if (d.m_curveId != -1)
        world.setPendingDispEnd(entity, d.m_curveId);
    d.m_elapsedMs    = {};
    d.m_velocityTime = d.m_acceleration = d.m_accelerationTime = {};
    d.m_gravity                                                = Fixed::fromInt(1);
    d.m_bounces                                                = {};
    d.m_velocity                                               = {};
    d.m_curveId                                                = -1;
}
}  // namespace rules
}  // namespace mugen::sim::displacement
