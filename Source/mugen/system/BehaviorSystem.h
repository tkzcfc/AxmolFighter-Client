#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class BehaviorSystem : public System
{
public:
    typedef System Super;

    BehaviorSystem();
    virtual ~BehaviorSystem();

    virtual void init(ECSManager* ecs) override;
    virtual void onEntityAdded(Entity* entity) override;
    virtual void onEntityRemoved(Entity* entity) override;
    virtual void update() override;

private:
    void processPendingHits(Entity* entity);
    void selectBranch(Entity* entity);
    void tickLocomotion(Entity* entity, int32_t dtMs);
    void tickAttack(Entity* entity, int32_t dtMs);
    void tryCastFromInput(Entity* entity);
};

NS_MG_END
