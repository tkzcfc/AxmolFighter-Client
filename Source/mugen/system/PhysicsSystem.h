#pragma once

#include "mugen/core/ecs/System.h"
#include "mugen/core/math/Vec2.h"
#include "mugen/core/math/Vec3.h"

NS_MG_BEGIN

class PhysicsSystem : public System
{
public:
    typedef System Super;

public:
    PhysicsSystem();
    virtual ~PhysicsSystem();

    virtual void init(ECSManager* ecs) override;
    virtual void onEntityAdded(Entity* entity) override;
    virtual void onEntityRemoved(Entity* entity) override;
    virtual void update() override;

    Vector2f mapMin;
    Vector2f mapMax;
    Vector3f gravity;
};

NS_MG_END
