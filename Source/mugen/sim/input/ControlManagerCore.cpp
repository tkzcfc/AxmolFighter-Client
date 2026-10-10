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

// 从互斥方向键状态得出当前方向编码。
int32_t direction(const std::array<bool, 20>& held)
{
    const bool left  = held[keyIndex(ControlKey::Left)];
    const bool right = held[keyIndex(ControlKey::Right)];
    const bool up    = held[keyIndex(ControlKey::Up)];
    const bool down  = held[keyIndex(ControlKey::Down)];
    if (right && !left)
        return 1;
    if (left && !right)
        return 3;
    if (down && !up)
        return 2;
    if (up && !down)
        return 4;
    return 0;
}
}  // namespace

// 记录按键变化，并在攻击键重新按下时重置重复计时。
void ControlManagerCore::setKey(ControlKey key, bool pressed)
{
    const size_t index = keyIndex(key);
    if (m_held[index] == pressed)
        return;
    m_held[index] = pressed;
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

// 处理方向变化、推进连招，并在方向归零时发出待机。
void ControlManagerCore::updateDirection(std::vector<PlayerCommand>& commands)
{
    const int32_t current = direction(m_held);
    if (current == m_lastDirection)
        return;
    m_lastDirection = current;
    if (current == 0)
    {
        // 两轴都松开后清掉上一帧留下的移动方向。
        commands.push_back({ControlEvent::CTR_IDLE, 0, 0, 0});
        return;
    }
    m_comboWait = kJoystickWaitFrames;
    for (size_t index = 0; index < kCombos.size(); ++index)
    {
        const uint8_t progress = m_comboProgress[index];
        if (kCombos[index][progress] == current)
            ++m_comboProgress[index];
        else
            m_comboProgress[index] = kCombos[index][0] == current ? 1 : 0;
    }
    const int32_t angle = current == 1 || current == 3 ? 0 : 90000;
    commands.push_back({ControlEvent::CTR_RUN, current, angle, 1});
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
