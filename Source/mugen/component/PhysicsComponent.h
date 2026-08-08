#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/math/Vec3.h"

NS_MG_BEGIN

class PhysicsComponent : public Component
{
public:
    typedef Component Super;

public:
    PhysicsComponent() {}
    virtual ~PhysicsComponent() {}

    // 是否为静态物体，静态物体不受物理影响但可与其他物体发生碰撞。
    bool isStaticBody = true;

    // 物体的宽高，用于碰撞检测和物理计算。
    Vector2f size;

    // 物理层使用的高精度位置。
    // TransformComponent 里的整数坐标由 PhysicsSystem 每帧同步回去。
    Vector3f position;

    // 当前速度：
    // x = 水平移动/击退速度
    // y = 预留（目前未启用）
    // z = 跳跃/击飞速度
    Vector3f velocity;

    // 重力加速度。
    float gravity = 1800.0f;

    // 重力缩放系数（技能阶段可设为 0 实现空中悬停）。
    float gravityScale = 1.0f;

    // 是否位于地面上。
    int8_t onGround = 1;

    // 地面高度（z 轴着地判断基准）。
    float groundLevel = 0.0f;

    // 各轴最大速度绝对值限制（仅限制主动移动速度 velocity）。
    Vector3f maxVelocity = {500.0f, 500.0f, 1500.0f};

    // 落地后冲量速度（impulseVelocity）的指数衰减系数（单位：1/秒）。
    // 物理系统每帧：impulse *= exp(-friction * dt)
    // 参考值：5 = 约 0.6s 停止（慢），15 = 约 0.2s 停止（中），30 = 约 0.1s 停止（快）。
    float friction = 10.0f;

    // 由战斗系统施加的冲量速度（击退/击飞），与主动移动速度独立。
    // PhysicsSystem 每帧在落地后按 friction 自动衰减，外部只需写入初始值。
    Vector3f impulseVelocity;

    // 上一帧位置，由 PhysicsSystem 每帧自动更新，不参与序列化。
    Vector3f lastPosition;

    MG_DEFINE_SERIALIZABLE(isStaticBody,
                           size,
                           position,
                           velocity,
                           gravity,
                           gravityScale,
                           onGround,
                           groundLevel,
                           maxVelocity,
                           friction,
                           impulseVelocity,
                           lastPosition);
};

NS_MG_END
