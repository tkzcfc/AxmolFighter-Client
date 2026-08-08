#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

// 镜头：焦点居中 → viewPos，指数平滑拉近；地图根 setPosition(viewPos)
class VirtualCamera
{
public:
    VirtualCamera();

    static std::unique_ptr<VirtualCamera> create();

    bool init();

    void setViewPortSize(const ax::Size& viewSize);
    void setRegion(const ax::Size& regionSize);
    void setWorldSize(const ax::Size& worldSize) { setRegion(worldSize); }

    void setFocusPosition(const ax::Vec2& focusPos);
    void setTargetPosition(const ax::Vec2& targetPos) { setFocusPosition(targetPos); }

    void snapToFocus();

    const ax::Vec2& getViewPosition() const { return m_glPosition; }
    const ax::Vec2& getFocusPosition() const { return m_focusPosition; }

    void setCall(const std::function<void(float, float, float)>& call) { m_call = call; }

    void doUpdate(float delta);

public:
    MG_SYNTHESIZE(float, m_zoom, Zoom);
    MG_SYNTHESIZE_IS(bool, m_enableCollision, EnableCollision);
    // 指数衰减跟随速度（越大越快跟上）。<=0 瞬时跟随。推荐 3~12。
    MG_SYNTHESIZE(float, m_smoothSpeed, SmoothSpeed);
    MG_SYNTHESIZE_PASS_BY_REF(ax::Vec2, m_anchorPoint, AnchorPoint);

private:
    ax::Vec2 computeIdealViewPosition() const;
    void clampViewPosition(ax::Vec2& viewPos) const;
    void applyCall();

private:
    ax::Size m_viewSize;
    ax::Size m_regionSize;

    ax::Vec2 m_focusPosition;
    ax::Vec2 m_glPosition;

    bool m_hasFocus = false;
    ax::Vec2 m_cachePos;
    float m_cacheScale = 0.0f;

    std::function<void(float, float, float)> m_call;
};

NS_MG_END
