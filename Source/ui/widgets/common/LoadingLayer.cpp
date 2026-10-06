#include "LoadingLayer.h"

#include <algorithm>

namespace gameui
{

LoadingLayer::LoadingLayer()
{
    auto* object = UIPackage::createObject("Common", "LoadingLayer");
    m_root       = object ? object->as<GComponent>() : nullptr;
    if (m_root)
    {
        if (auto* child = m_root->getChild("progressBar"))
            m_progressBar = child->as<GProgressBar>();
        if (m_progressBar)
            m_progressBar->setValue(0.0);
    }
}

void LoadingLayer::setTarget(float progress01)
{
    m_target = std::clamp(progress01, 0.0f, 1.0f);
}

void LoadingLayer::finish()
{
    m_finished = true;
}

void LoadingLayer::update(float dt)
{
    m_smoother.update(dt, m_target, m_finished);
    if (m_progressBar)
        m_progressBar->setValue(m_smoother.getDisplay() * m_progressBar->getMax());
}

}  // namespace gameui
