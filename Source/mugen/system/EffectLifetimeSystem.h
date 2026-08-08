#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class EffectLifetimeSystem : public System
{
public:
    typedef System Super;

    EffectLifetimeSystem();
    virtual ~EffectLifetimeSystem();

    virtual void init(ECSManager* ecs) override;
    virtual void update() override;
};

NS_MG_END
