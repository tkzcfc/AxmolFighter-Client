#pragma once
#include "mugen/sim/input/PlayerCommand.h"
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"
#include <vector>

namespace mugen::sim
{
// 玩家控制组件：未消费命令按输入顺序保存，解冻首帧按原顺序消费（R9）。
struct PlayerControl
{
    // 玩家槽位。
    uint32_t m_slot = 0;
    // 未消费的命令队列。
    std::vector<PlayerCommand> m_commands;
    MG_REFLECT(PlayerControl, m_slot, m_commands)
};
// 移动意图模式。
enum class MoveMode : uint8_t
{
    // 无移动意图。
    None = 0,
    // 停止。
    Stop,
    // 行走。
    Walk,
    // 跑动。
    Run,
    // 移动到目标点。
    MoveTo
};
// 单帧移动意图。
struct MoveIntent
{
    // 移动模式。
    MoveMode m_mode = MoveMode::None;
    // 方向象限。
    ControlQuadrant m_quadrant = ControlQuadrant::None;
    // 方向角（1/1000 度）。
    int32_t m_angle = 0;
    // MoveTo 的目标位置。
    FixedVec3 m_targetPos;
    MG_REFLECT(MoveIntent, m_mode, m_quadrant, m_angle, m_targetPos)
};
// 单条按键意图。
struct ButtonIntent
{
    // 控制事件。
    ControlEvent m_event = ControlEvent::CTR_NONE;
    // 按下类型。
    int32_t m_pressType = 0;
    // 方向角（1/1000 度）。
    int32_t m_angle = 0;
    MG_REFLECT(ButtonIntent, m_event, m_pressType, m_angle)
};
// 意图组件：PlayerIntent/Ai 每帧重写，供 SkillRequest / RoleFsm / Locomotion 读取。
struct Intent
{
    // 移动意图。
    MoveIntent m_move;
    // 按键意图（施法，B2 使用）。
    std::vector<ButtonIntent> m_buttons;
    // 是否由 AI 自动施法。
    bool m_autoCast = false;
    // 目标实体。
    Entity m_target;
    MG_REFLECT(Intent, m_move, m_buttons, m_autoCast, m_target)
};
}  // namespace mugen::sim
