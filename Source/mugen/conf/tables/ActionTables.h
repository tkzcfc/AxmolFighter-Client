#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// action 表映射的角色动作配置。
struct ActionConfig
{
    // 动画名称或索引。
    std::variant<int32_t, std::string> action;
    // 动画延迟、朝向、落点类型与播放倍率。
    int32_t actionDelayTime   = 0;
    int32_t actionOrientation = 0;
    int32_t actionPosType     = 0;
    Fixed actionScaleTime;
    // 动作附加效果与镜头触发帧。
    std::vector<int32_t> buffIds;
    std::vector<int32_t> cameraFrame;
    std::vector<int32_t> cameraId;
    // 操作控制方式与可控移动速度。
    int32_t control = 0;
    Fixed controlVelocity;
    // 自定义方向及展示控制参数。
    std::vector<int32_t> customVec;
    int32_t dialogShow             = 0;
    int32_t displacementId         = 0;
    int32_t displaySpineFrame      = 0;
    int32_t displaySpineFrameCount = 0;
    std::vector<std::vector<int32_t>> displaySpineIds;
    // 动作生成的特效及其触发帧。
    std::vector<int32_t> effectFrames;
    std::vector<std::variant<int32_t, std::string>> effectIds;
    // 特效继承方向、贴地与残影标记。
    int32_t extendRoleVec       = 0;
    int32_t floor               = 0;
    int32_t ghost               = 0;
    // 配置行 id 与动作打断帧。
    int32_t id                  = 0;
    int32_t interruptExtraFrame = 0;
    int32_t interruptFrame      = 0;
    // 循环、名字显示、阻挡和相对位置参数。
    Fixed loop;
    int32_t nameShow = 0;
    int32_t obstruct = 0;
    std::vector<Fixed> relativeActionPos;
    // 阴影、音效与定身镜头参数。
    Fixed shadow;
    std::vector<int32_t> soundId;
    int32_t staticResetTime  = 0;
    int32_t staticStartFrame = 0;
    int32_t staticTarget     = 0;
    int32_t staticTime       = 0;
    // 提示显示与变身参数。
    int32_t tipsShow         = 0;
    int32_t transformFrame   = 0;
    int32_t transformId      = 0;
    int32_t transformType    = 0;

    MG_REFLECT(ActionConfig,
               action,
               actionDelayTime,
               actionOrientation,
               actionPosType,
               actionScaleTime,
               buffIds,
               cameraFrame,
               cameraId,
               control,
               controlVelocity,
               customVec,
               dialogShow,
               displacementId,
               displaySpineFrame,
               displaySpineFrameCount,
               displaySpineIds,
               effectFrames,
               effectIds,
               extendRoleVec,
               floor,
               ghost,
               id,
               interruptExtraFrame,
               interruptFrame,
               loop,
               nameShow,
               obstruct,
               relativeActionPos,
               shadow,
               soundId,
               staticResetTime,
               staticStartFrame,
               staticTarget,
               staticTime,
               tipsShow,
               transformFrame,
               transformId,
               transformType)
};

// action_effect 表映射的特效动作配置。
struct ActionEffectConfig
{
    // 动画与播放控制参数。
    std::variant<int32_t, std::string> action;
    int64_t actionDelayTime = 0;
    std::variant<Fixed, std::string> actionScaleTime;
    // 镜头触发帧与镜头配置 id。
    std::vector<int32_t> cameraFrame;
    std::vector<int32_t> cameraId;
    std::variant<int32_t, std::string> control;
    // 可控速度与位移配置 id。
    Fixed controlVelocity;
    int32_t displacementId = 0;
    // 子特效及其触发帧。
    std::vector<int32_t> effectFrames;
    std::vector<int32_t> effectIds;
    // 贴地、残影、标识和打断参数。
    int32_t floor          = 0;
    int32_t ghost          = 0;
    int32_t id             = 0;
    int32_t interruptFrame = 0;
    int64_t loop           = 0;
    // 阻挡、阴影及动作音效参数。
    int32_t obstruct       = 0;
    Fixed shadow;
    std::vector<int32_t> soundId;
    std::variant<std::vector<std::vector<int32_t>>, std::vector<std::vector<std::string>>> soundId2;
    // 召唤角色的触发帧、配置 id 和存活时间。
    int32_t summonFrame = 0;
    int32_t summonId    = 0;
    int64_t summonTime  = 0;

    MG_REFLECT(ActionEffectConfig,
               action,
               actionDelayTime,
               actionScaleTime,
               cameraFrame,
               cameraId,
               control,
               controlVelocity,
               displacementId,
               effectFrames,
               effectIds,
               floor,
               ghost,
               id,
               interruptFrame,
               loop,
               obstruct,
               shadow,
               soundId,
               soundId2,
               summonFrame,
               summonId,
               summonTime)
};

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
