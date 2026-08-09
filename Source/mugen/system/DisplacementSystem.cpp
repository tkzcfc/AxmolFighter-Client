#include "DisplacementSystem.h"
#include "mugen/Components.h"

#include <algorithm>

NS_MG_BEGIN

DisplacementSystem::DisplacementSystem() {}
DisplacementSystem::~DisplacementSystem() {}

void DisplacementSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, DisplacementComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, PhysicsComponent);
}

namespace
{
void restoreGravity(DisplacementComponent* disp, PhysicsComponent* physics)
{
    if (disp && physics && disp->hasSavedGravity)
    {
        physics->gravityScale = disp->savedGravityScale;
        disp->hasSavedGravity = false;
    }
}
}  // namespace

void DisplacementSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    const float dt     = dtMs / 1000.0f;
    for (Entity* entity : entities)
    {
        auto* disp      = MG_GET_COMPONENT(entity, DisplacementComponent);
        auto* physics   = MG_GET_COMPONENT(entity, PhysicsComponent);
        auto* transform = MG_GET_COMPONENT(entity, TransformComponent);
        if (!disp || !physics)
            continue;
        if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        {
            if (attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0)
                continue;
        }

        if (disp->finished || !disp->activeConfig)
            continue;

        if (!disp->hasSavedGravity)
        {
            disp->savedGravityScale = physics->gravityScale;
            disp->hasSavedGravity   = true;
        }

        disp->elapsedMs += dtMs;
        const auto* cfg = disp->activeConfig;

        const int32_t maxT = std::max({cfg->velocityTime.x, cfg->velocityTime.y, cfg->velocityTime.z,
                                       cfg->accelerationTime.x, cfg->accelerationTime.y, cfg->accelerationTime.z});
        if (maxT > 0 && disp->elapsedMs >= maxT)
        {
            disp->finished        = true;
            physics->velocity.x   = 0;
            physics->velocity.y   = 0;
            // z 留给落地物理；还原重力缩放
            restoreGravity(disp, physics);
            continue;
        }

        const float facing = transform && transform->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;

        if (cfg->velocityTime.x <= 0 || disp->elapsedMs <= cfg->velocityTime.x)
            disp->velocity.x = cfg->velocity.x;
        if (cfg->velocityTime.y <= 0 || disp->elapsedMs <= cfg->velocityTime.y)
            disp->velocity.y = cfg->velocity.y;
        if (cfg->velocityTime.z <= 0 || disp->elapsedMs <= cfg->velocityTime.z)
            disp->velocity.z = cfg->velocity.z;

        if (cfg->accelerationTime.x <= 0 || disp->elapsedMs <= cfg->accelerationTime.x)
            disp->acceleration.x = cfg->acceleration.x;
        if (cfg->accelerationTime.y <= 0 || disp->elapsedMs <= cfg->accelerationTime.y)
            disp->acceleration.y = cfg->acceleration.y;
        if (cfg->accelerationTime.z <= 0 || disp->elapsedMs <= cfg->accelerationTime.z)
            disp->acceleration.z = cfg->acceleration.z;

        disp->velocity.x += disp->acceleration.x * dt;
        disp->velocity.y += disp->acceleration.y * dt;
        disp->velocity.z += disp->acceleration.z * dt;

        physics->velocity.x = disp->velocity.x * facing * 1000.0f;
        physics->velocity.y = disp->velocity.y * 1000.0f;
        physics->velocity.z = disp->velocity.z * 1000.0f;
        if (cfg->gravity != 0.0f)
            physics->gravityScale = cfg->gravity;

        if (cfg->bounces > 0 && physics->onGround && disp->velocity.z < 0)
            disp->velocity.z = std::abs(disp->velocity.z) * 0.6f;
    }
}

NS_MG_END
