#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec3.h"

namespace mugen::sim
{
// 角色世界坐标、朝向和缩放。
struct TransformComponent
{
    // 三维逻辑坐标。
    FixedVec3 m_position;
    // 水平朝向，仅取 -1 或 1。
    int32_t m_towardX = 1;
    // 朝向对应的 X 方向向量。
    Fixed m_vectorX = Fixed::fromInt(1);
    // 朝向对应的 Z 方向向量。
    Fixed m_vectorZ;
    // 逻辑碰撞框缩放倍率。
    Fixed m_scale = Fixed::fromInt(1);
    MG_REFLECT(TransformComponent, m_position, m_towardX, m_vectorX, m_vectorZ, m_scale)
};
// 角色位移的可快照动力学状态（移植自旧实现 DisplacementComponent.h，速度字段统一放这里）。
struct DisplacementComponent
{
    // 当前位移已推进的毫秒数。
    Fixed m_elapsedMs;
    // 三轴速度保持时长、加速度、加速时长和本帧实际位移。
    FixedVec3 m_velocityTime;
    FixedVec3 m_acceleration;
    FixedVec3 m_accelerationTime;
    FixedVec3 m_offset;
    // 移动状态下三轴速度缩放。
    FixedVec3 m_velocityScale{Fixed::fromInt(1), Fixed::fromInt(1), Fixed::fromInt(1)};
    // 非移动状态下的默认速度缩放。
    Fixed m_defaultVelocityScale = Fixed::fromInt(1);
    // 垂直方向重力倍率。
    Fixed m_gravity = Fixed::fromInt(1);
    // 落地反弹倍率。
    Fixed m_bounces;
    // 逻辑时间的额外缩放倍率。
    Fixed m_extraTimeScale = Fixed::fromInt(1);
    // 当前三轴速度：Locomotion/FSM 写控制速度，DisplacementSystem 积分后写回。
    FixedVec3 m_velocity;
    // 当前位移配置标识（-1 无）。
    int32_t m_curveId = -1;
    // 本帧位移事件位（R11），下一帧由状态体/FSM 读取。
    bool m_brake = false;
    bool m_air = false;
    bool m_floor = false;
    bool m_lie = false;
    bool m_wall = false;
    // 位移策略：落地 / 撞墙 / brake 结束片段（数据化，后续切片使用）。
    int32_t m_floorPolicy = 0;
    int32_t m_wallPolicy = 0;
    bool m_endOnBrake = false;
    // 落地不反弹（浮空值耗尽时由 HitReactionSystem 写入，DisplacementSystem 当帧读取）。
    bool m_noFloorBounce = false;
    MG_REFLECT(DisplacementComponent, m_elapsedMs, m_velocityTime, m_acceleration, m_accelerationTime, m_offset, m_velocityScale,
               m_defaultVelocityScale, m_gravity, m_bounces, m_extraTimeScale, m_velocity, m_curveId,
               m_brake, m_air, m_floor, m_lie, m_wall, m_floorPolicy, m_wallPolicy, m_endOnBrake, m_noFloorBounce)
};
}  // namespace mugen::sim
