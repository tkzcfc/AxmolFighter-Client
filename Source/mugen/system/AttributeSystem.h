#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class AttributeSystem : public System
{
public:
    typedef System Super;

public:
    AttributeSystem();

    virtual ~AttributeSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;

    virtual void update() override;
};

NS_MG_END
