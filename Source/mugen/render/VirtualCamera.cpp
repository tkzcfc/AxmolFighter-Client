#include "VirtualCamera.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

std::unique_ptr<VirtualCamera> VirtualCamera::create()
{
    auto ret = std::make_unique<VirtualCamera>();
    if (ret && ret->init())
        return ret;
    return nullptr;
}

VirtualCamera::VirtualCamera()
    : m_enableCollision(true), m_zoom(1.0f), m_anchorPoint(ax::Vec2::ANCHOR_MIDDLE), m_smoothSpeed(3.0f)
{}

bool VirtualCamera::init()
{
    m_viewSize   = ax::Director::getInstance()->getVisibleSize();
    m_regionSize = m_viewSize;
    return true;
}

void VirtualCamera::setViewPortSize(const ax::Size& viewSize)
{
    m_viewSize = viewSize;
}

void VirtualCamera::setRegion(const ax::Size& regionSize)
{
    m_regionSize = regionSize;
}

ax::Vec2 VirtualCamera::computeIdealViewPosition() const
{
    // 锚点居中：地图根位移使焦点落在视口锚点
    ax::Vec2 viewPos;
    viewPos.x = m_viewSize.width * m_anchorPoint.x - m_focusPosition.x * m_zoom;
    viewPos.y = m_viewSize.height * m_anchorPoint.y - m_focusPosition.y * m_zoom;
    clampViewPosition(viewPos);
    return viewPos;
}

void VirtualCamera::setFocusPosition(const ax::Vec2& focusPos)
{
    m_focusPosition = focusPos;
    if (!m_hasFocus)
    {
        m_hasFocus = true;
        snapToFocus();
    }
}

void VirtualCamera::snapToFocus()
{
    m_glPosition = computeIdealViewPosition();
    applyCall();
}

void VirtualCamera::clampViewPosition(ax::Vec2& viewPos) const
{
    if (!m_enableCollision)
        return;

    // viewPos 语义：地图根 setPosition(viewPos)。地图比视口大时夹在 [view-map, 0]；
    // 地图比视口小时该区间倒置，改为居中锁定，避免每帧两端跳闪。
    const float minOffsetX = m_viewSize.width - m_regionSize.width * m_zoom;
    const float minOffsetY = m_viewSize.height - m_regionSize.height * m_zoom;

    if (minOffsetX <= 0.0f)
        viewPos.x = std::clamp(viewPos.x, minOffsetX, 0.0f);
    else
        viewPos.x = minOffsetX * 0.5f;

    if (minOffsetY <= 0.0f)
        viewPos.y = std::clamp(viewPos.y, minOffsetY, 0.0f);
    else
        viewPos.y = minOffsetY * 0.5f;
}

void VirtualCamera::applyCall()
{
    if (!m_call)
        return;
    if (FLOAT_EQUAL(m_glPosition.x, m_cachePos.x) && FLOAT_EQUAL(m_glPosition.y, m_cachePos.y) &&
        FLOAT_EQUAL(m_cacheScale, m_zoom))
        return;

    m_cachePos   = m_glPosition;
    m_cacheScale = m_zoom;
    m_call(m_glPosition.x, m_glPosition.y, m_zoom);
}

void VirtualCamera::doUpdate(float delta)
{
    if (!m_hasFocus)
        return;

    const ax::Vec2 targetView = computeIdealViewPosition();

    // 与旧版一致：指数衰减平滑拉近（帧率无关）
    if (m_smoothSpeed > 0.0f && delta > 0.0f)
    {
        const float t = 1.0f - std::exp(-m_smoothSpeed * delta);
        m_glPosition.x += (targetView.x - m_glPosition.x) * t;
        m_glPosition.y += (targetView.y - m_glPosition.y) * t;
    }
    else
    {
        m_glPosition = targetView;
    }

    clampViewPosition(m_glPosition);
    applyCall();
}

NS_MG_END
