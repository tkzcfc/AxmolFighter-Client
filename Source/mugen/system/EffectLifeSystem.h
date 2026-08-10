#pragma once

#include "mugen/core/ecs/System.h"
#include "mugen/core/ecs/Types.h"

NS_MG_BEGIN

class TransformComponent;

class EffectLifeSystem : public System
{
public:
    typedef System Super;

    EffectLifeSystem();
    virtual ~EffectLifeSystem();

    void init(ECSManager* ecs) override;
    void update() override;

    static Entity* spawnEffect(ECSManager* ecs,
                               int32_t effectId,
                               EntityId ownerId,
                               const TransformComponent* originTf,
                               int32_t skillHitId,
                               bool chainFromParent = false);

    static void spawnHitEffects(Entity* effectEntity, Entity* hitTarget);
};

NS_MG_END
