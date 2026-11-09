#pragma once
#include <cstdint>

namespace mugen::sim
{
// 出生阶段（R3）：记录实体由哪个同步点创建，用于出生帧内的行动门控。
// 数值越大越晚；大于 RoleTimeline 的实体在出生帧内不参与逐实体推进。
enum class SpawnStage : uint8_t
{
    // 角色时间轴之后的同步点：本帧参与特效遍、位移、动画。
    RoleTimeline = 0,
    // 特效时间轴之后的同步点：下一帧起行动。
    EffectTimeline = 1,
    // 战斗阶段之后的同步点。
    Combat = 2,
    // 生命周期阶段之后的同步点。
    Lifecycle = 3,
    // 房间阶段之后的同步点。
    Room = 4
};
}  // namespace mugen::sim
