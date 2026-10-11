#include "ControlManagerCore.h"

namespace mugen::sim
{
namespace
{
// 攻击键到控制事件的固定映射。
constexpr std::array<ControlEvent, 16> kAttackEvents = {
    ControlEvent::CTR_ATKA, ControlEvent::CTR_ATKB, ControlEvent::CTR_ATKC, ControlEvent::CTR_ATKD,
    ControlEvent::CTR_ATKE, ControlEvent::CTR_ATKF, ControlEvent::CTR_ATKG, ControlEvent::CTR_ATKH,
    ControlEvent::CTR_ATKI, ControlEvent::CTR_ATKJ, ControlEvent::CTR_ATKK, ControlEvent::CTR_ATKL,
    ControlEvent::CTR_ATKM, ControlEvent::CTR_ATKN, ControlEvent::CTR_ATKO, ControlEvent::CTR_ATKQ,
};
// 用方向编码描述的可识别连招序列。
constexpr std::array<std::array<int32_t, 5>, 24> kCombos = {
    {{2, 3, 4, 1, 2}, {4, 3, 2, 1, 4}, {3, 2, 3, 2, 0}, {3, 4, 3, 4, 0}, {2, 4, 3, 2, 0}, {4, 2, 3, 4, 0},
     {2, 3, 4, 0, 0}, {4, 3, 2, 0, 0}, {3, 2, 1, 0, 0}, {3, 4, 1, 0, 0}, {3, 4, 0, 0, 0}, {3, 2, 0, 0, 0},
     {2, 4, 0, 0, 0}, {4, 2, 0, 0, 0}, {3, 1, 0, 0, 0}, {3, 1, 0, 0, 0}, {3, 4, 3, 2, 0}, {3, 2, 3, 4, 0},
     {2, 1, 4, 3, 2}, {4, 1, 2, 3, 4}, {1, 3, 0, 0, 0}, {1, 3, 0, 0, 0}, {2, 4, 2, 0, 0}, {4, 2, 4, 0, 0}}};

// 将枚举键转换为按键状态数组下标。
constexpr size_t keyIndex(ControlKey key)
{
    return static_cast<size_t>(key);
}

// 一帧方向意图：象限仍是四向扇区，斜向额外带 45 度角。
struct DirectionAim
{
    int32_t quadrant = 0;
    int32_t angle    = 0;
};

// 同一轴两侧的按住状态与按下序号。
struct AxisKeys
{
    bool negativeHeld      = false;
    bool positiveHeld      = false;
    uint32_t negativeOrder = 0;
    uint32_t positiveOrder = 0;
};

// 同轴对向键同时按住时，后按下的一侧决定符号。
int32_t axisSign(AxisKeys axis)
{
    if (axis.negativeHeld && axis.positiveHeld)
        return axis.positiveOrder >= axis.negativeOrder ? 1 : -1;
    if (axis.positiveHeld)
        return 1;
    if (axis.negativeHeld)
        return -1;
    return 0;
}

// 斜向象限与移动叶子的向量符号一致：右上 1、右下 2、左下 3、左上 4。
int32_t diagonalQuadrant(int32_t xSign, int32_t zSign)
{
    if (xSign > 0)
        return zSign > 0 ? 1 : 2;
    return zSign > 0 ? 4 : 3;
}

// 按各轴后按下的方向键得出象限；垂直两轴同时按住时角度为 45 度。
DirectionAim direction(const std::array<bool, 20>& held, const std::array<uint32_t, 4>& pressOrder)
{
    const int32_t xSign = axisSign({held[keyIndex(ControlKey::Left)], held[keyIndex(ControlKey::Right)],
                                    pressOrder[keyIndex(ControlKey::Left)], pressOrder[keyIndex(ControlKey::Right)]});
    const int32_t zSign = axisSign({held[keyIndex(ControlKey::Down)], held[keyIndex(ControlKey::Up)],
                                    pressOrder[keyIndex(ControlKey::Down)], pressOrder[keyIndex(ControlKey::Up)]});
    if (xSign == 0 && zSign == 0)
        return {};
    if (xSign != 0 && zSign != 0)
        return {diagonalQuadrant(xSign, zSign), 45000};
    if (xSign != 0)
        return {xSign > 0 ? 1 : 3, 0};
    return {zSign > 0 ? 4 : 2, 90000};
}
}  // namespace

// 记录按键变化，并在攻击键重新按下时重置重复计时。
void ControlManagerCore::setKey(ControlKey key, bool pressed)
{
    const size_t index = keyIndex(key);
    if (m_held[index] == pressed)
        return;
    m_held[index] = pressed;
    if (pressed && index < m_pressOrder.size())
        m_pressOrder[index] = ++m_pressSerial;
    if (index >= keyIndex(ControlKey::AttackA) && pressed)
        m_attackRepeat = 0;
}

// 优先发出完成的连招，否则追加普通攻击。
void ControlManagerCore::appendAttack(ControlEvent event, int32_t pressType, std::vector<PlayerCommand>& commands)
{
    if (event == ControlEvent::CTR_ATKA)
    {
        const ControlEvent combo = comboEvent();
        if (combo != ControlEvent::CTR_NONE)
        {
            commands.push_back({combo, 0, 0, pressType});
            resetCombo();
            return;
        }
    }
    commands.push_back({event, 0, 0, pressType});
}

// 取消当前所有连招匹配状态。
void ControlManagerCore::resetCombo()
{
    m_comboProgress.fill(0);
    m_comboWait = 0;
}

// 查找首个已经完全匹配的方向连招。
ControlEvent ControlManagerCore::comboEvent() const
{
    for (size_t index = 0; index < m_comboProgress.size(); ++index)
    {
        size_t length = 0;
        while (length < kCombos[index].size() && kCombos[index][length] != 0)
            ++length;
        if (m_comboProgress[index] >= length)
            return static_cast<ControlEvent>(static_cast<uint8_t>(ControlEvent::CTR_JOY1) + index / 2);
    }
    return ControlEvent::CTR_NONE;
}

// 方向或角度变化时发令；仍有方向键按住时不发待机。
void ControlManagerCore::updateDirection(std::vector<PlayerCommand>& commands)
{
    const DirectionAim aim = direction(m_held, m_pressOrder);
    if (aim.quadrant == m_lastDirection && aim.angle == m_lastAngle)
        return;
    const bool sectorChanged = aim.quadrant != m_lastDirection;
    m_lastDirection          = aim.quadrant;
    m_lastAngle              = aim.angle;
    if (aim.quadrant == 0)
    {
        // 两轴都松开后清掉上一帧留下的移动方向。
        commands.push_back({ControlEvent::CTR_IDLE, 0, 0, 0});
        return;
    }
    if (sectorChanged)
    {
        m_comboWait = kJoystickWaitFrames;
        for (size_t index = 0; index < kCombos.size(); ++index)
        {
            const uint8_t progress = m_comboProgress[index];
            if (kCombos[index][progress] == aim.quadrant)
                ++m_comboProgress[index];
            else
                m_comboProgress[index] = kCombos[index][0] == aim.quadrant ? 1 : 0;
        }
    }
    commands.push_back({ControlEvent::CTR_RUN, aim.quadrant, aim.angle, 1});
}

// 按固定顺序处理方向、连招超时和攻击重复。
void ControlManagerCore::advance(std::vector<PlayerCommand>& commands)
{
    updateDirection(commands);
    if (m_comboWait != 0 && --m_comboWait == 0)
        resetCombo();
    for (size_t index = 0; index < kAttackEvents.size(); ++index)
    {
        const bool held = m_held[keyIndex(ControlKey::AttackA) + index];
        if (!held)
            continue;
        if (index != 0 || m_attackRepeat == 0)
            appendAttack(kAttackEvents[index], 1, commands);
    }
    if (m_held[keyIndex(ControlKey::AttackA)])
        m_attackRepeat = (m_attackRepeat + 1) % kAttackRepeatFrames;
    else
        m_attackRepeat = 0;
}
}  // namespace mugen::sim
