#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class BuffSystem : public System
{
public:
    typedef System Super;
    BuffSystem();
    virtual ~BuffSystem();
    virtual void init(ECSManager* ecs) override;
    virtual void update() override;
};

NS_MG_END
