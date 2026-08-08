#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class AvatarRenderSystem : public System
{
public:
    typedef System Super;

public:
    AvatarRenderSystem();

    virtual ~AvatarRenderSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void update() override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;
};

NS_MG_END
