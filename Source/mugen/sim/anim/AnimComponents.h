#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec3.h"
#include <string>
#include <vector>

namespace mugen::sim
{
// 可快照的值类型动作播放状态。
struct MotionStateComponent
{
    // 动作文件，更新阶段据此重建临时播放器。
    std::string motionFile;
    // 当前动作名称。
    std::string motionName;
    // 当前动作入口标识。
    std::string entryId;
    // 当前动作播放时间。
    Fixed timeMs;
    // 动作时间缩放倍率。
    Fixed rate = Fixed::fromInt(1);
    // 是否循环播放。
    bool loop = true;
    // 是否正在播放。
    bool playing = true;
    // 已完成循环次数。
    int64_t completedLoops = 0;
    // 本帧是否完成（R10）：AnimationSystem 写，时间轴/FSM 下一帧读。
    bool completedThisFrame = false;
    MG_REFLECT(MotionStateComponent, motionFile, motionName, entryId, timeMs, rate, loop, playing, completedLoops,
               completedThisFrame)
};
// 位于世界坐标的轴对齐碰撞框。X 为横向，Y 为纵深，Z 为高度。
struct WorldBox
{
    // 三轴最小角。
    FixedVec3 pos;
    // 三轴尺寸。
    FixedVec3 size;
    MG_REFLECT(WorldBox, pos, size)
};
// 角色当前帧登记的攻击和受击框（HitboxSystem 采样）。
struct HitboxesComponent
{
    // 攻击碰撞框列表。
    std::vector<WorldBox> attack;
    // 受击碰撞框列表。
    std::vector<WorldBox> damage;
    // 行走、贴墙和推挤半径，不参与命中。
    Fixed radius;
    // 命中深度全长。0 表示采用动作框自身的深度。
    Fixed depth;
    // 本次采样发生的逻辑帧。
    uint64_t sampledFrame = 0;
    MG_REFLECT(HitboxesComponent, attack, damage, radius, depth, sampledFrame)
};
}  // namespace mugen::sim
