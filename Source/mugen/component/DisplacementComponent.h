#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/math/Vec3.h"
#include "mugen/conf/TableConfig.h"
#include "mugen/component/PhysicsComponent.h"

NS_MG_BEGIN

class DisplacementComponent : public Component
{
public:
    typedef Component Super;

    DisplacementComponent() {}
    virtual ~DisplacementComponent() {}

    // 表空间速度（每毫秒）→ 物理速度（每秒）
    static constexpr float kLuaVelToPhysics = 1000.0f;

    const DisplacementConfig* activeConfig = nullptr;
    int32_t activeId                       = 0;
    int32_t elapsedMs                      = 0;
    // 表空间速度：x=水平，y=高度，z=纵深（写入物理时映射到 x/z/y）
    Vector3f velocity;
    Vector3f acceleration;
    bool finished    = true;
    bool braked      = false;
    bool airEvent    = false;
    bool lieEvent    = false;
    float facingSign = 1.0f;
    // start 前缓存，reset 时还原 PhysicsComponent::gravityScale
    float savedGravityScale = 1.0f;
    bool hasSavedGravity    = false;

    bool isActive() const { return !finished && activeConfig != nullptr; }

    void reset()
    {
        activeConfig      = nullptr;
        activeId          = 0;
        elapsedMs         = 0;
        velocity          = Vector3f{};
        acceleration      = Vector3f{};
        finished          = true;
        braked            = false;
        airEvent          = false;
        lieEvent          = false;
        facingSign        = 1.0f;
        hasSavedGravity   = false;
        savedGravityScale = 1.0f;
    }

    void restoreGravity(PhysicsComponent* physics)
    {
        if (physics && hasSavedGravity)
            physics->gravityScale = savedGravityScale;
        hasSavedGravity = false;
    }

    void start(const DisplacementConfig* cfg, float facing = 1.0f)
    {
        // 保留已缓存的重力，避免受击位移覆盖技能位移时把 gravityScale 丢成 0
        const bool savedG  = hasSavedGravity;
        const float savedS = savedGravityScale;
        reset();
        hasSavedGravity   = savedG;
        savedGravityScale = savedS;
        facingSign        = facing;
        if (!cfg)
            return;
        activeConfig = cfg;
        activeId     = cfg->id;
        velocity     = cfg->velocity;
        acceleration = cfg->acceleration;
        finished     = false;
        braked       = false;
        airEvent     = false;
        lieEvent     = false;
    }

    void writePhysicsVelocity(PhysicsComponent* physics, float facingSign) const
    {
        if (!physics || finished)
            return;
        physics->velocity.x = velocity.x * facingSign * kLuaVelToPhysics;
        physics->velocity.y = velocity.z * kLuaVelToPhysics;
        physics->velocity.z = velocity.y * kLuaVelToPhysics;
    }

    MG_DEFINE_SERIALIZABLE(activeId,
                           elapsedMs,
                           velocity,
                           acceleration,
                           finished,
                           braked,
                           airEvent,
                           lieEvent,
                           facingSign,
                           savedGravityScale,
                           hasSavedGravity);
};

NS_MG_END
