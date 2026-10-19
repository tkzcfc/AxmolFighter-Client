#pragma once
#include "mugen/conf/GameDef.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim
{
// 写入帧输入的单条角色控制命令。
struct PlayerCommand
{
    // 触发的控制事件。
    ControlEvent controlEvent = ControlEvent::CTR_NONE;
    // 方向象限编码。
    int32_t quadrant = 0;
    // 角度乘以 1000 的整数值，不经渲染数值解码。
    int32_t angle = 0;
    // 按下类型编码。
    int32_t pressType = 0;
    // 参与帧输入归档的命令字段。
    MG_REFLECT(PlayerCommand, controlEvent, quadrant, angle, pressType)
};
}  // namespace mugen::sim
