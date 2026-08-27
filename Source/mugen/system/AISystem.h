#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

// 只负责 ensure/bind/restore 后 tick AiAgent；选技与巡逻状态都在对象图上。
class AISystem : public System
{
public:
    typedef System Super;
    AISystem();
    virtual ~AISystem();
    virtual void init(ECSManager* ecs) override;
    virtual void onEntityAdded(Entity* entity) override;
    virtual void update() override;
};

NS_MG_END
