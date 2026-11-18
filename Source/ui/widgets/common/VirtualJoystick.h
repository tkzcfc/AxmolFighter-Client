#pragma once

#include "FairyGUI.h"
#include "mugen/sim/input/ControlManagerCore.h"

#include <array>
#include <functional>

namespace gameui
{

// 虚拟摇杆控制器：绑定 FairyGUI Joystick 组件（含 touchArea 与 thumb），
// 按死区阈值将拖拽偏移映射为四个方向的按下/抬起状态。
class VirtualJoystick
{
public:
    using DirectionCallback = std::function<void(mugen::sim::ControlKey key, bool pressed)>;

    // 摇杆死区占半径的比例。
    static constexpr float kDeadZone = 0.25f;

    bool bind(fairygui::GComponent* joystick, DirectionCallback onDirectionChanged);
    void release();

    bool isDirectionPressed(mugen::sim::ControlKey key) const;
    const std::array<bool, 4>& directions() const { return m_direction; }

private:
    void update(const ax::Vec2& stagePos);
    void setDirection(bool left, bool right, bool up, bool down);

    fairygui::GComponent* m_joystick = nullptr;
    fairygui::GObject* m_thumb       = nullptr;
    ax::Vec2 m_thumbOrigin{};
    int m_touchId = -1;
    std::array<bool, 4> m_direction{};
    DirectionCallback m_onDirectionChanged;
};

}  // namespace gameui
