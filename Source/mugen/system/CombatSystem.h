#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class CombatSystem : public System
{
public:
    typedef System Super;

public:
    CombatSystem();

    virtual ~CombatSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;

    virtual void update() override;
};

NS_MG_END
