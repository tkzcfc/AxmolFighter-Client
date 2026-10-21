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
// 将箭头序列编译成象限连招；未写满的位置视为结束。
constexpr std::array<ControlQuadrant, 5> comboOf(char32_t a,
                                                 char32_t b = 0,
                                                 char32_t c = 0,
                                                 char32_t d = 0,
                                                 char32_t e = 0)
{
    return {quadrantFromArrow(a), quadrantFromArrow(b), quadrantFromArrow(c), quadrantFromArrow(d),
            quadrantFromArrow(e)};
}

// 用上下左右箭头描述的可识别连招序列。
constexpr std::array<std::array<ControlQuadrant, 5>, kDirectionComboCount> kCombos = {{
    // CTR_JOY1
    comboOf(U'↓', U'←', U'↑', U'→', U'↓'),
    comboOf(U'↑', U'←', U'↓', U'→', U'↑'),

    // CTR_JOY2
    comboOf(U'←', U'↓', U'←', U'↓'),
    comboOf(U'←', U'↑', U'←', U'↑'),

    // CTR_JOY3
    comboOf(U'↓', U'↑', U'←', U'↓'),
    comboOf(U'↑', U'↓', U'←', U'↑'),

    // CTR_JOY4
    comboOf(U'↓', U'←', U'↑'),
    comboOf(U'↑', U'←', U'↓'),

    // CTR_JOY5
    comboOf(U'←', U'↓', U'→'),
    comboOf(U'←', U'↑', U'→'),

    // CTR_JOY6
    comboOf(U'←', U'↑'),
    comboOf(U'←', U'↓'),

    // CTR_JOY7
    comboOf(U'↓', U'↑'),
    comboOf(U'↑', U'↓'),

    // CTR_JOY8
    comboOf(U'←', U'→'),
    comboOf(U'←', U'→'),

    // CTR_JOY9
    comboOf(U'←', U'↑', U'←', U'↓'),
    comboOf(U'←', U'↓', U'←', U'↑'),

    // CTR_JOY10
    comboOf(U'↓', U'→', U'↑', U'←', U'↓'),
    comboOf(U'↑', U'→', U'↓', U'←', U'↑'),

    // CTR_JOY11
    comboOf(U'→', U'←'),
    comboOf(U'→', U'←'),

    // CTR_JOY12
    comboOf(U'↓', U'↑', U'↓'),
    comboOf(U'↑', U'↓', U'↑'),
}};

// 将枚举键转换为按键状态数组下标。
constexpr size_t keyIndex(ControlKey key)
{
    return static_cast<size_t>(key);
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
            commands.push_back({combo, ControlQuadrant::None, 0, pressType});
            resetCombo();
            return;
        }
    }
    commands.push_back({event, ControlQuadrant::None, 0, pressType});
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
        while (length < kCombos[index].size() && kCombos[index][length] != ControlQuadrant::None)
            ++length;
        if (m_comboProgress[index] >= length)
            return static_cast<ControlEvent>(static_cast<uint8_t>(ControlEvent::CTR_JOY1) + index / 2);
    }
    return ControlEvent::CTR_NONE;
}

// 方向或角度变化时发令；仍有方向键按住时不发待机。
void ControlManagerCore::updateDirection(std::vector<PlayerCommand>& commands)
{
    const ControlAim aim = ControlAim::fromDirectionKeys(
        m_held[keyIndex(ControlKey::Left)], m_held[keyIndex(ControlKey::Right)], m_held[keyIndex(ControlKey::Down)],
        m_held[keyIndex(ControlKey::Up)], m_pressOrder[keyIndex(ControlKey::Left)],
        m_pressOrder[keyIndex(ControlKey::Right)], m_pressOrder[keyIndex(ControlKey::Down)],
        m_pressOrder[keyIndex(ControlKey::Up)]);
    // 按键的象限和角度都没变时不重新发出命令，避免重复发令。
    if (aim.quadrant == m_lastDirection && aim.angle == m_lastAngle)
        return;
    const bool sectorChanged = aim.quadrant != m_lastDirection;
    m_lastDirection          = aim.quadrant;
    m_lastAngle              = aim.angle;
    if (aim.quadrant == ControlQuadrant::None)
    {
        // 没有方向键按住时发出待机命令。
        commands.push_back({ControlEvent::CTR_IDLE, ControlQuadrant::None, 0, 0});
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
