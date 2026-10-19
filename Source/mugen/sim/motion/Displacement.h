#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/conf/tables/ActionTables.h"

namespace mugen::sim
{
// 位移系统所更新的世界实例。
class World;
// 位移过程中通知行为系统的状态变化。
enum class DisplacementEvent : uint8_t
{
    Brake,
    Air,
    Floor,
    Lie,
    Left,
    Right,
    Top,
    Bottom
};
// 角色位移的可快照动力学状态。
struct Displacement
{
    // 当前位移已推进的毫秒数。
    Fixed elapsedMs;
    // 三轴速度保持时长、加速度、加速时长和本帧实际位移。
    FixedVec3 velocityTime, acceleration, accelerationTime, offset;
    // 移动状态下三轴速度缩放。
    FixedVec3 velocityScale{Fixed::fromInt(1), Fixed::fromInt(1), Fixed::fromInt(1)};
    // 非移动状态下的默认速度缩放。
    Fixed defaultVelocityScale = Fixed::fromInt(1);
    // 垂直方向重力倍率。
    Fixed gravity = Fixed::fromInt(1);
    // 落地反弹倍率。
    Fixed bounces;
    // 逻辑时间的额外缩放倍率。
    Fixed extraTimeScale = Fixed::fromInt(1);
    // 接收位移事件的行为树节点标识。
    int32_t listenerNode = -1;
    // 参与组件快照的位移字段。
    MG_REFLECT(Displacement,
               elapsedMs,
               velocityTime,
               acceleration,
               accelerationTime,
               offset,
               velocityScale,
               defaultVelocityScale,
               gravity,
               bounces,
               extraTimeScale,
               listenerNode)
};
namespace displacement
{
// 单轴积分后得到的位移和下一速度。
struct AxisStep
{
    Fixed offset, velocity;
};
// 按加速度、速度时长和重力积分单轴运动。
AxisStep integrate(Fixed time,
                   Fixed velocity,
                   Fixed acceleration,
                   Fixed accelerationTime,
                   Fixed velocityTime,
                   Fixed dt,
                   Fixed gravity = {});
// 实体位移的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class DisplacementDriver
{
public:
    // 构造指向指定世界实例中某个实体的视图。
    DisplacementDriver(World& world, Entity entity);
    // 清空实体的位移和受击强制速度。
    void reset();
    // 按配置标识设置位移，配置缺失时返回 false。
    bool set(int32_t id, int32_t listenerNode = -1);
    // 用已解析的配置填充实体位移状态。
    void setData(const conf::DisplacementConfig& data, int32_t listenerNode = -1);
    // 在朝向扇形内查找最近的可追踪目标。
    Entity findTargetInSector(Fixed radius, Fixed angle);
    // 推进实体一帧位移并处理地面和区域边界。
    void update();

private:
    World& m_world;
    Entity m_entity{};
};
}  // namespace displacement
}  // namespace mugen::sim
