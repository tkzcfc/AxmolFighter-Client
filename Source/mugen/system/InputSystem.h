#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class InputSystem : public System
{
public:
    typedef System Super;

public:
    InputSystem();

    virtual ~InputSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;

    virtual void update() override;
};

NS_MG_END
