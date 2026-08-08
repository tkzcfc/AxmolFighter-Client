#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class GameMapSystem : public System
{
public:
    typedef System Super;

public:
    GameMapSystem();
    virtual ~GameMapSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;
};

NS_MG_END
