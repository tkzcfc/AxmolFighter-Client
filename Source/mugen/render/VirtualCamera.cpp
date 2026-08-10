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

void VirtualCamera::shake(float amplitude, float durationMs, int32_t /*freezeTimeMs*/)
{
    if (amplitude <= 0.0f || durationMs <= 0.0f)
        return;
    m_shaking          = true;
    m_shakeAmplitude   = amplitude;
    m_shakeDurationMs  = durationMs;
    m_shakeElapsedMs   = 0.0f;
    m_shakeCycles      = 2.0f;
    m_shakeOffset      = ax::Vec2::ZERO;
}

void VirtualCamera::updateShake(float deltaSec)
{
    if (!m_shaking)
    {
        m_shakeOffset = ax::Vec2::ZERO;
        return;
    }

    m_shakeElapsedMs += deltaSec * 1000.0f;
    if (m_shakeElapsedMs >= m_shakeDurationMs)
    {
        m_shaking     = false;
        m_shakeOffset = ax::Vec2::ZERO;
        return;
    }

    const float percent = m_shakeElapsedMs / m_shakeDurationMs;
    const float angle   = m_shakeCycles * 2.0f * 3.14159265f * percent;
    // 衰减：后半段减弱
    const float factor = 1.0f - percent;
    m_shakeOffset.x    = m_shakeAmplitude * std::cos(angle) * factor;
    m_shakeOffset.y    = m_shakeAmplitude * std::sin(angle) * factor;
}

void VirtualCamera::clampViewPosition(ax::Vec2& viewPos) const
{
    if (!m_enableCollision)
        return;

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
    const float ox = m_glPosition.x + m_shakeOffset.x;
    const float oy = m_glPosition.y + m_shakeOffset.y;
    if (FLOAT_EQUAL(ox, m_cachePos.x) && FLOAT_EQUAL(oy, m_cachePos.y) && FLOAT_EQUAL(m_cacheScale, m_zoom))
        return;

    m_cachePos.x = ox;
    m_cachePos.y = oy;
    m_cacheScale = m_zoom;
    m_call(ox, oy, m_zoom);
}

void VirtualCamera::doUpdate(float delta)
{
    if (!m_hasFocus)
        return;

    updateShake(delta);

    const ax::Vec2 targetView = computeIdealViewPosition();

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
