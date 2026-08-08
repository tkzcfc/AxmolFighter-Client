#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class AISystem : public System
{
public:
    typedef System Super;
    AISystem();
    virtual ~AISystem();
    virtual void init(ECSManager* ecs) override;
    virtual void update() override;
};

NS_MG_END
