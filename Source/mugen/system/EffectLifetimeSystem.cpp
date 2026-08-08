#include "EffectLifetimeSystem.h"

#include "mugen/Components.h"

NS_MG_BEGIN

EffectLifetimeSystem::EffectLifetimeSystem() {}
EffectLifetimeSystem::~EffectLifetimeSystem() {}

void EffectLifetimeSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, EffectLifetimeComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
}

void EffectLifetimeSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    for (Entity* entity : entities)
    {
        auto* fx = MG_GET_COMPONENT(entity, EffectLifetimeComponent);
        auto* tf = MG_GET_COMPONENT(entity, TransformComponent);
        if (!fx || !tf)
            continue;

        if (fx->follow && fx->ownerId != INVALID_ENTITY_ID)
        {
            if (auto* owner = getECSManager()->getEntity(fx->ownerId))
            {
                if (auto* ownerTf = MG_GET_COMPONENT(owner, TransformComponent))
                {
                    const float facing  = ownerTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
                    tf->position.x      = ownerTf->position.x + static_cast<int32_t>(fx->relativePosition.x * facing);
                    tf->position.y      = ownerTf->position.y + static_cast<int32_t>(fx->relativePosition.y);
                    tf->position.z      = ownerTf->position.z + static_cast<int32_t>(fx->relativePosition.z);
                    tf->facingDirection = ownerTf->facingDirection;
                }
            }
        }

        fx->elapsedMs += dtMs;
        if (fx->lifetimeMs > 0 && fx->elapsedMs >= fx->lifetimeMs)
            getECSManager()->destroyEntity(entity);
    }
}

NS_MG_END
