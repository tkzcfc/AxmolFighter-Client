#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/math/Vec3.h"
#include "mugen/conf/TableConfig.h"

NS_MG_BEGIN

class DisplacementComponent : public Component
{
public:
    typedef Component Super;

    DisplacementComponent() {}
    virtual ~DisplacementComponent() {}

    const DisplacementConfig* activeConfig = nullptr;
    int32_t activeId                       = 0;
    int32_t elapsedMs                      = 0;
    Vector3f velocity;
    Vector3f acceleration;
    bool finished = true;

    void reset()
    {
        activeConfig = nullptr;
        activeId     = 0;
        elapsedMs    = 0;
        velocity     = Vector3f{};
        acceleration = Vector3f{};
        finished     = true;
    }

    void start(const DisplacementConfig* cfg)
    {
        reset();
        if (!cfg)
            return;
        activeConfig = cfg;
        activeId     = cfg->id;
        velocity     = cfg->velocity;
        acceleration = cfg->acceleration;
        finished     = false;
    }

    MG_DEFINE_SERIALIZABLE(activeId, elapsedMs, velocity, acceleration, finished);
};

NS_MG_END
