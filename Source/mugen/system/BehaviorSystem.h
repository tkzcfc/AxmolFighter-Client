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
    void tickHitRecovery(Entity* entity, int32_t dtMs);
    void tickSkillCooldowns(Entity* entity, int32_t dtMs);
    void tryCastFromInput(Entity* entity);
    void selectBranch(Entity* entity);
    void tickLocomotion(Entity* entity, int32_t dtMs);
    void tickAttack(Entity* entity, int32_t dtMs);
    void updateAirborneTags(Entity* entity);
    void updateDoubleTapRun(Entity* entity, int64_t runningTimeMs);
    bool beginSkill(Entity* entity, int32_t skillId, int32_t inputSlot, int32_t stepInSlot);
    int32_t resolveSkillFromSlot(Entity* entity, int32_t inputSlot, int32_t* outStep) const;
    bool canInterruptActive(int32_t pendingId, int32_t activeId, bool interruptOpen, bool interruptExtraOpen) const;
    bool tryAbortAttackForLocomotion(Entity* entity);
};

NS_MG_END
