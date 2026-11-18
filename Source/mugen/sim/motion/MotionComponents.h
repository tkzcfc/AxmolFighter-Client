#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec3.h"

namespace mugen::sim
{
// 角色世界坐标、朝向和缩放。
struct TransformComponent
{
    // 三维逻辑坐标。
    FixedVec3 position;
    // 水平朝向，仅取 -1 或 1。
    int32_t towardX = 1;
    // 朝向对应的 X 方向向量。
    Fixed vectorX = Fixed::fromInt(1);
    // 朝向对应的 Z 方向向量。
    Fixed vectorZ;
    // 逻辑碰撞框缩放倍率。
    Fixed scale = Fixed::fromInt(1);
    MG_REFLECT(TransformComponent, position, towardX, vectorX, vectorZ, scale)
};
// 角色位移的可快照动力学状态（移植自旧实现 DisplacementComponent.h，速度字段统一放这里）。
struct DisplacementComponent
{
    // 当前位移已推进的毫秒数。
    Fixed elapsedMs;
    // 三轴速度保持时长、加速度、加速时长和本帧实际位移。
    FixedVec3 velocityTime;
    FixedVec3 acceleration;
    FixedVec3 accelerationTime;
    FixedVec3 offset;
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
    // 当前三轴速度：Locomotion/FSM 写控制速度，DisplacementSystem 积分后写回。
    FixedVec3 velocity;
    // 当前位移配置标识（-1 无）。
    int32_t curveId = -1;
    // 本帧位移事件位（R11），下一帧由状态体/FSM 读取。
    bool brake = false;
    bool air = false;
    bool floor = false;
    bool lie = false;
    bool wall = false;
    // 位移策略：落地 / 撞墙 / brake 结束片段（数据化，后续切片使用）。
    int32_t floorPolicy = 0;
    int32_t wallPolicy = 0;
    bool endOnBrake = false;
    // 落地不反弹（浮空值耗尽时由 HitReactionSystem 写入，DisplacementSystem 当帧读取）。
    bool noFloorBounce = false;
    MG_REFLECT(DisplacementComponent, elapsedMs, velocityTime, acceleration, accelerationTime, offset, velocityScale,
               defaultVelocityScale, gravity, bounces, extraTimeScale, velocity, curveId,
               brake, air, floor, lie, wall, floorPolicy, wallPolicy, endOnBrake, noFloorBounce)
};
}  // namespace mugen::sim
