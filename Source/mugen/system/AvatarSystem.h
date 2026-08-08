#pragma once

#include "mugen/core/ecs/System.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class AvatarSystem : public System
{
public:
    typedef System Super;

public:
    AvatarSystem();

    virtual ~AvatarSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void update() override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;
};

NS_MG_END
