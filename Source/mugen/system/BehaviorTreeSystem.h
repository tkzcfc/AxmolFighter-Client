#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

class BehaviorTreeSystem : public System
{
public:
    typedef System Super;

    BehaviorTreeSystem();
    virtual ~BehaviorTreeSystem();

    virtual void init(ECSManager* ecs) override;
    virtual void onEntityAdded(Entity* entity) override;
    virtual void update() override;

private:
    void fillContext(Entity* entity, struct BTContext& ctx);
    void tryCastFromInput(Entity* entity);
    void processPendingHits(Entity* entity);
    void tickHitRecovery(Entity* entity, int32_t dtMs);
    void tickSkillCooldowns(Entity* entity, int32_t dtMs);
    void updateAirborneTags(Entity* entity);
    void updateDoubleTapRun(Entity* entity, int64_t runningTimeMs);
};

NS_MG_END
