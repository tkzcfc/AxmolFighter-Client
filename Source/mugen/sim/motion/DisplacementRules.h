#pragma once
#include "mugen/core/math/Fixed.h"
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim
{
class World;
namespace displacement
{
// 单轴积分后得到的位移和下一速度。
struct AxisStep
{
    Fixed m_offset;
    Fixed m_velocity;
};
// 按加速度、速度时长和重力积分单轴运动（移植旧实现 displacement::integrate）。
AxisStep integrate(Fixed time,
                   Fixed velocity,
                   Fixed acceleration,
                   Fixed accelerationTime,
                   Fixed velocityTime,
                   Fixed dt,
                   Fixed gravity = {});
namespace rules
{
// 按配置标识设置位移状态；配置缺失返回 false。替换活动曲线时发出 Replaced，成功后发出 Began。
bool set(World& world, Entity entity, int32_t id);
// 重置位移状态与速度。活动曲线被清空时发出 Ended。
void reset(World& world, Entity entity);
// 速度与加速度都耗尽时发出 Ended，并清掉活动曲线 id。
void settle(World& world, Entity entity);
}  // namespace rules
}  // namespace displacement
}  // namespace mugen::sim
