#include "DisplacementSystem.h"
#include "mugen/Components.h"

#include <algorithm>
#include <cmath>

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
// 表空间高度重力（每毫秒）
constexpr float kLuaGravityPerMs = -0.0022f;

int accelApplyMs(int elapsedMs, int dtMs, int accelTime)
{
    if (accelTime < 0)
        return dtMs;
    if (accelTime <= 0)
        return 0;
    if (elapsedMs <= accelTime)
        return dtMs;
    const int prev = elapsedMs - dtMs;
    if (prev < accelTime)
        return accelTime - prev;
    return 0;
}

void integrateAxis(float& vel, float accel, int accelTime, int velTime, int elapsedMs, int dtMs)
{
    vel += accel * static_cast<float>(accelApplyMs(elapsedMs, dtMs, accelTime));
    if (velTime > 0 && elapsedMs > velTime)
        vel = 0.0f;
}
}  // namespace

void DisplacementSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    if (dtMs <= 0)
        return;

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
        // 位移期间由本系统积分高度重力，避免与物理重力叠两次
        physics->gravityScale = 0.0f;

        const auto* cfg = disp->activeConfig;
        disp->elapsedMs += dtMs;

        integrateAxis(disp->velocity.x, cfg->acceleration.x, cfg->accelerationTime.x, cfg->velocityTime.x,
                      disp->elapsedMs, dtMs);
        integrateAxis(disp->velocity.z, cfg->acceleration.z, cfg->accelerationTime.z, cfg->velocityTime.z,
                      disp->elapsedMs, dtMs);

        const int heightAccelMs = accelApplyMs(disp->elapsedMs, dtMs, cfg->accelerationTime.y);
        disp->velocity.y += cfg->acceleration.y * static_cast<float>(heightAccelMs);
        if (cfg->velocityTime.y > 0 && disp->elapsedMs > cfg->velocityTime.y && cfg->gravity == 0.0f)
            disp->velocity.y = 0.0f;

        const bool grounded = physics->onGround != 0 && physics->position.z <= physics->groundLevel + 0.01f;
        if (cfg->gravity != 0.0f)
        {
            if (grounded && disp->velocity.y <= 0.0f)
                disp->velocity.y = 0.0f;
            else
                disp->velocity.y += cfg->gravity * kLuaGravityPerMs * static_cast<float>(dtMs);
        }

        const float facing = disp->facingSign;
        disp->writePhysicsVelocity(physics, facing);

        // 物理已在本帧积分过：起跳当帧把高度写进位置，否则要等下一帧才离地
        if (physics->onGround && disp->velocity.y > 0.0f)
        {
            const float dtSec = static_cast<float>(dtMs) / 1000.0f;
            physics->position.z =
                physics->groundLevel + disp->velocity.y * DisplacementComponent::kLuaVelToPhysics * dtSec;
            physics->onGround   = 0;
            physics->justLanded = false;
            if (transform)
                transform->position.z = static_cast<int32_t>(physics->position.z);
            disp->airEvent = true;
        }

        if (!grounded)
            disp->airEvent = true;
        if (grounded && disp->elapsedMs > 0)
            disp->lieEvent = true;

        disp->braked = std::abs(disp->velocity.x) < 1e-6f && std::abs(disp->velocity.y) < 1e-6f &&
                       std::abs(disp->velocity.z) < 1e-6f;

        if (cfg->bounces > 0 && grounded && disp->velocity.y < 0.0f)
            disp->velocity.y = std::abs(disp->velocity.y) * cfg->bounces;
    }
}

NS_MG_END
