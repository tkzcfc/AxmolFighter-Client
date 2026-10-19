#pragma once

#include "PlayerCommand.h"

namespace mugen::sim
{
// 本地控制器可追踪的方向键和攻击键。
enum class ControlKey : uint8_t
{
    // 向左方向键。
    Left,
    // 向右方向键。
    Right,
    // 向上方向键。
    Up,
    // 向下方向键。
    Down,
    // 攻击键 A。
    AttackA,
    // 攻击键 B。
    AttackB,
    // 攻击键 C。
    AttackC,
    // 攻击键 D。
    AttackD,
    // 攻击键 E。
    AttackE,
    // 攻击键 F。
    AttackF,
    // 攻击键 G。
    AttackG,
    // 攻击键 H。
    AttackH,
    // 攻击键 I。
    AttackI,
    // 攻击键 J。
    AttackJ,
    // 攻击键 K。
    AttackK,
    // 攻击键 L。
    AttackL,
    // 攻击键 M。
    AttackM,
    // 攻击键 N。
    AttackN,
    // 攻击键 O。
    AttackO,
    // 攻击键 Q。
    AttackQ,
    // 枚举数量；不是有效按键。
    Count,
};

// 所有控制键当前是否按住。
using ControlHeldState = std::array<bool, static_cast<size_t>(ControlKey::Count)>;

// 可识别的方向连招序列条数（与 kCombos 对齐）。
inline constexpr size_t kDirectionComboCount = 24;
// 每种方向连招已匹配的输入长度。
using DirectionComboProgress = std::array<uint8_t, kDirectionComboCount>;

// 将按键状态转换为确定性角色控制命令。
class ControlManagerCore
{
public:
    // 按住攻击 A 时重复发令的帧间隔。
    static constexpr uint32_t kAttackRepeatFrames = 6;
    // 方向连招输入允许的最大等待帧数。
    static constexpr uint32_t kJoystickWaitFrames = 9;

    // 更新单个按键的按下状态，并记录方向键的按下先后。
    void setKey(ControlKey key, bool pressed);
    // 推进一帧并追加本帧生成的命令。
    void advance(std::vector<PlayerCommand>& commands);

private:
    // 按需将攻击事件或完成的连招写入命令列表。
    void appendAttack(ControlEvent event, int32_t pressType, std::vector<PlayerCommand>& commands);
    // 检测方向或角度变化，并在象限变化时推进连招。
    void updateDirection(std::vector<PlayerCommand>& commands);
    // 清空所有连招进度和等待计时。
    void resetCombo();
    // 返回已匹配完成的首个连招事件。
    ControlEvent comboEvent() const;

    // 所有控制键当前是否按住。
    ControlHeldState m_held{};
    // 每种方向连招已匹配的输入长度。
    DirectionComboProgress m_comboProgress{};
    // 攻击 A 的重复发令计时。
    uint32_t m_attackRepeat = 0;
    // 连招输入超时计时。
    uint32_t m_comboWait = 0;
    // 上一帧已处理的方向编码。
    int32_t m_lastDirection = 0;
    // 上一帧已发出的移动角度，角度变化时即使象限不变也重新发令。
    int32_t m_lastAngle = 0;
    // 四个方向键的按下序号，同轴对向键同时按住时序号更大的一侧生效。
    std::array<uint32_t, 4> m_pressOrder{};
    // 方向键按下序号计数，只在本实例上递增。
    uint32_t m_pressSerial = 0;
};
}  // namespace mugen::sim
