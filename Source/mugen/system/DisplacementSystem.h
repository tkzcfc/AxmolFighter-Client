#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class DisplacementSystem : public System
{
public:
    typedef System Super;
    DisplacementSystem();
    virtual ~DisplacementSystem();
    virtual void init(ECSManager* ecs) override;
    virtual void update() override;
};

NS_MG_END
