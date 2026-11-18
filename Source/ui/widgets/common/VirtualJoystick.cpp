#include "ui/widgets/common/VirtualJoystick.h"

namespace gameui
{

bool VirtualJoystick::bind(fairygui::GComponent* joystick, DirectionCallback onDirectionChanged)
{
    m_joystick           = nullptr;
    m_thumb              = nullptr;
    m_touchId            = -1;
    m_direction          = {};
    m_onDirectionChanged = std::move(onDirectionChanged);

    if (!joystick)
        return false;

    auto* thumb = joystick->getChild("thumb");
    auto* area  = joystick->getChild("touchArea");
    if (!thumb || !area)
        return false;

    m_joystick    = joystick;
    m_thumb       = thumb;
    m_thumbOrigin = m_thumb->getPosition();

    area->addEventListener(fairygui::UIEventType::TouchBegin, [this](fairygui::EventContext* context) {
        if (m_touchId != -1)
            return;
        m_touchId = context->getInput()->getTouchId();
        context->captureTouch();
        update(context->getInput()->getPosition());
    });
    area->addEventListener(fairygui::UIEventType::TouchMove, [this](fairygui::EventContext* context) {
        if (context->getInput()->getTouchId() == m_touchId)
            update(context->getInput()->getPosition());
    });
    area->addEventListener(fairygui::UIEventType::TouchEnd, [this](fairygui::EventContext* context) {
        if (context->getInput()->getTouchId() == m_touchId)
            release();
    });
    return true;
}

void VirtualJoystick::update(const ax::Vec2& stagePos)
{
    const ax::Vec2 center = m_joystick->localToGlobal(m_thumbOrigin + m_thumb->getSize() * 0.5f);
    const float radius    = m_joystick->getWidth() * 0.5f;
    ax::Vec2 delta        = m_joystick->globalToLocal(stagePos) - m_joystick->globalToLocal(center);
    if (delta.length() > radius)
        delta = delta.getNormalized() * radius;
    m_thumb->setPosition(m_thumbOrigin.x + delta.x, m_thumbOrigin.y + delta.y);
    const float dead = radius * kDeadZone;
    // FairyGUI 的 y 轴向下，向上推对应 Up。
    setDirection(delta.x < -dead, delta.x > dead, delta.y < -dead, delta.y > dead);
}

void VirtualJoystick::release()
{
    m_touchId = -1;
    if (m_thumb)
        m_thumb->setPosition(m_thumbOrigin.x, m_thumbOrigin.y);
    setDirection(false, false, false, false);
}

void VirtualJoystick::setDirection(bool left, bool right, bool up, bool down)
{
    const std::array<bool, 4> next = {left, right, up, down};
    const std::array<mugen::sim::ControlKey, 4> keys = {
        mugen::sim::ControlKey::Left,
        mugen::sim::ControlKey::Right,
        mugen::sim::ControlKey::Up,
        mugen::sim::ControlKey::Down,
    };
    for (size_t i = 0; i < next.size(); ++i)
    {
        if (next[i] == m_direction[i])
            continue;
        m_direction[i] = next[i];
        if (m_onDirectionChanged)
            m_onDirectionChanged(keys[i], next[i]);
    }
}

bool VirtualJoystick::isDirectionPressed(mugen::sim::ControlKey key) const
{
    switch (key)
    {
    case mugen::sim::ControlKey::Left:
        return m_direction[0];
    case mugen::sim::ControlKey::Right:
        return m_direction[1];
    case mugen::sim::ControlKey::Up:
        return m_direction[2];
    case mugen::sim::ControlKey::Down:
        return m_direction[3];
    default:
        return false;
    }
}

}  // namespace gameui
