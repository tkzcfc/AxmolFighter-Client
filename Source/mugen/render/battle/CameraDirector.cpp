#include "CameraDirector.h"

#ifdef RUNTIME_IN_AXMOL

#    include <algorithm>

NS_MG_BEGIN

namespace
{
float renderValue(Fixed value)
{
    return value.toFloatForRender();
}
}  // namespace

void CameraDirector::follow(sim_legacy::World& world, Entity entity)
{
    const auto* transform = world.ecs().tryGet<sim_legacy::Transform>(entity);
    if (!transform || !m_map.camera())
        return;
    m_map.camera()->setFocusPosition({renderValue(transform->position.x), renderValue(transform->position.z)});
}

void CameraDirector::consume(const sim_legacy::PresentationEvent& event)
{
    if (const auto* shake = std::get_if<sim_legacy::CameraShake>(&event.payload); shake && m_map.camera())
        m_map.camera()->shake(renderValue(shake->strength), renderValue(shake->durationMs));
    if (std::holds_alternative<sim_legacy::DisplaySpine>(event.payload))
        m_displaySlowScale = 0.35f;
}

void CameraDirector::update(float delta)
{
    m_displaySlowScale = std::min(1.0f, m_displaySlowScale + delta * 2.0f);
    m_map.update(delta * m_displaySlowScale);
}

NS_MG_END

#endif
