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
// 发出一条位移事实。终点由 DebugSinkSystem 在 P11 读 TransformComponent（与旧导出 afterTick 取帧末位置一致）。
void emit(World& world, Entity entity, DispPhase phase, int32_t curveId)
{
    DisplacementEvent event;
    event.entity  = entity;
    event.phase   = phase;
    event.curveId = curveId;
    world.ecs().singleton<EventQueue<DisplacementEvent>>().push(event);
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
    auto& d        = world.ecs().get<DisplacementComponent>(entity);
    auto& velocity = d.velocity;
    const auto& t  = world.ecs().get<TransformComponent>(entity);
    // 旧实现每次设置位移都先结束进行中的位移（同 id 也结束），再开始新位移。
    if (d.curveId != -1)
        emit(world, entity, DispPhase::Replaced, d.curveId);
    // B2 不实现追踪位移（isTrace），按普通三轴配置。
    velocity            = {t.vectorX * axis(row->velocity, 0), axis(row->velocity, 1),
                           t.vectorZ * axis(row->velocity, 2)};
    d.velocityTime    = {axis(row->velocityTime, 0), axis(row->velocityTime, 1), axis(row->velocityTime, 2)};
    d.acceleration    = {axis(row->acceleration, 0), axis(row->acceleration, 1), axis(row->acceleration, 2)};
    d.accelerationTime = {axis(row->accelerationTime, 0), axis(row->accelerationTime, 1),
                            axis(row->accelerationTime, 2)};
    d.gravity         = row->gravity;
    d.bounces         = row->bounces;
    d.elapsedMs       = {};
    d.curveId         = id;
    emit(world, entity, DispPhase::Began, id);
    return true;
}

// 重置位移状态与速度。
void reset(World& world, Entity entity, bool endAirborne)
{
    auto& d = world.ecs().get<DisplacementComponent>(entity);
    const bool airborne = world.ecs().get<TransformComponent>(entity).position.y > Fixed{};
    if (!endAirborne && airborne && d.curveId != -1)
    {
        d.elapsedMs    = {};
        d.velocityTime = d.acceleration = d.accelerationTime = {};
        d.gravity                                                = Fixed::fromInt(1);
        d.bounces                                                = {};
        d.velocity                                               = {};
        return;
    }
    if (d.curveId != -1)
        emit(world, entity, DispPhase::Ended, d.curveId);
    d.elapsedMs    = {};
    d.velocityTime = d.acceleration = d.accelerationTime = {};
    d.gravity                                                = Fixed::fromInt(1);
    d.bounces                                                = {};
    d.velocity                                               = {};
    d.curveId                                                = -1;
}

// 活动曲线耗尽后清掉 curveId：这是“当前没有曲线”的玩法标志，下一次 set 不会把已结束曲线再报成 Replaced。
void settle(World& world, Entity entity)
{
    auto& d = world.ecs().get<DisplacementComponent>(entity);
    if (d.curveId == -1)
        return;
    const bool moving = d.velocity.x != Fixed{} || d.velocity.y != Fixed{} || d.velocity.z != Fixed{};
    const bool accelerating = (d.acceleration.x != Fixed{} && d.elapsedMs < d.accelerationTime.x) ||
                              (d.acceleration.y != Fixed{} && d.elapsedMs < d.accelerationTime.y) ||
                              (d.acceleration.z != Fixed{} && d.elapsedMs < d.accelerationTime.z);
    if (moving || accelerating)
        return;
    const int32_t curveId = d.curveId;
    d.curveId           = -1;
    emit(world, entity, DispPhase::Ended, curveId);
}
}  // namespace rules
}  // namespace mugen::sim::displacement
