#pragma once
#include "mugen/core/ecs/Schedule.h"
#include <span>

namespace mugen::sim
{
class World;
// 帧阶段，数值即 SystemEntry::m_phase；与 DESIGN.md §3.1 的 P0..P12 对应。
enum class Phase : uint8_t
{
    // P0 帧开始：时钟与延迟动作。
    FrameBegin = 0,
    // P1 输入。
    Input = 1,
    // P2 计时。
    Tick = 2,
    // P3 意图。
    Intent = 3,
    // P4 行动：施法、状态机、时间轴。
    Action = 4,
    // P5 移动。
    Movement = 5,
    // P6 动画与受击框。
    Animation = 6,
    // P7 碰撞。
    Collision = 7,
    // P8 战斗。
    Combat = 8,
    // P9 生命周期。
    Lifecycle = 9,
    // P10 房间。
    Room = 10,
    // P11 表现。
    Present = 11,
    // P12 帧末。
    FrameEnd = 12
};
// 新 sim 的系统表行类型。
using SystemEntry = mugen::SystemEntry<World>;
// 唯一帧调度表（DESIGN.md §3.2），进程级只读。
extern const std::span<const SystemEntry> kSchedule;
}  // namespace mugen::sim
