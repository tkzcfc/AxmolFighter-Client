#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// camera 表映射的镜头震动配置。
struct CameraConfig
{
    // 横纵振幅、持续时间、标识、优先级、修饰器和次数。
    int32_t amplitudeX = 0;
    int32_t amplitudeY = 0;
    int32_t duration   = 0;
    int32_t id         = 0;
    int32_t level      = 0;
    std::string modifier;
    int32_t times = 0;

    MG_REFLECT(CameraConfig, amplitudeX, amplitudeY, duration, id, level, modifier, times)
};

// displacement 表映射的位移曲线配置。
struct DisplacementConfig
{
    // 加速度曲线及其持续时间。
    std::vector<Fixed> acceleration;
    std::vector<Fixed> accelerationTime;
    // 弹跳、重力、标识与追踪参数。
    Fixed bounces;
    Fixed gravity;
    int32_t id          = 0;
    int32_t isTrace     = 0;
    int32_t traceAngle  = 0;
    int32_t traceRadius = 0;
    Fixed traceVelocity;
    // 速度曲线及其持续时间。
    std::vector<Fixed> velocity;
    std::vector<Fixed> velocityTime;

    MG_REFLECT(DisplacementConfig,
               acceleration,
               accelerationTime,
               bounces,
               gravity,
               id,
               isTrace,
               traceAngle,
               traceRadius,
               traceVelocity,
               velocity,
               velocityTime)
};
}  // namespace mugen::conf
