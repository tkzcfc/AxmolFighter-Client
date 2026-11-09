#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec3.h"
#include <string>
#include <vector>

namespace mugen::sim
{
// 可快照的值类型动作播放状态。
struct MotionState
{
    // 动作文件，更新阶段据此重建临时播放器。
    std::string m_motionFile;
    // 当前动作名称。
    std::string m_motionName;
    // 当前动作入口标识。
    std::string m_entryId;
    // 当前动作播放时间。
    Fixed m_timeMs;
    // 动作时间缩放倍率。
    Fixed m_rate = Fixed::fromInt(1);
    // 是否循环播放。
    bool m_loop = true;
    // 是否正在播放。
    bool m_playing = true;
    // 已完成循环次数。
    int64_t m_completedLoops = 0;
    // 本帧是否完成（R10）：AnimationSystem 写，时间轴/FSM 下一帧读。
    bool m_completedThisFrame = false;
    MG_REFLECT(MotionState, m_motionFile, m_motionName, m_entryId, m_timeMs, m_rate, m_loop, m_playing, m_completedLoops,
               m_completedThisFrame)
};
// 位于世界坐标的轴对齐碰撞框。
struct WorldBox
{
    // 框体起点。
    FixedVec3 m_pos;
    // 框体尺寸。
    FixedVec3 m_size;
    MG_REFLECT(WorldBox, m_pos, m_size)
};
// 角色当前帧登记的攻击和受击框（HitboxSystem 采样）。
struct Hitboxes
{
    // 攻击碰撞框列表。
    std::vector<WorldBox> m_attack;
    // 受击碰撞框列表。
    std::vector<WorldBox> m_damage;
    // 深度碰撞半径。
    Fixed m_radius;
    // 本次采样发生的逻辑帧。
    uint64_t m_sampledFrame = 0;
    MG_REFLECT(Hitboxes, m_attack, m_damage, m_radius, m_sampledFrame)
};
}  // namespace mugen::sim
