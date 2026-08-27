#pragma once

#include "mugen/core/ecs/System.h"
#include "mugen/core/ecs/Types.h"
#include "mugen/core/math/Vec3.h"

NS_MG_BEGIN

class TransformComponent;

class EffectLifeSystem : public System
{
public:
    typedef System Super;

    EffectLifeSystem();
    virtual ~EffectLifeSystem();

    void init(ECSManager* ecs) override;
    void onEntityAdded(Entity* entity) override;
    void update() override;

    // 按 entity_effect 表生成；skillHitLookupId 与原先 spawn 参数相同
    static Entity* spawnEffect(ECSManager* ecs,
                               int32_t effectId,
                               EntityId ownerId,
                               const TransformComponent* originTf,
                               int32_t skillHitLookupId,
                               bool chainFromParent = false);

    // 无表行的展示特效（Buff 跟随 Spine）
    static Entity* spawnVisual(ECSManager* ecs,
                               int32_t resSpineId,
                               EntityId ownerId,
                               const TransformComponent* originTf,
                               bool follow,
                               int32_t lifetimeMs,
                               const Vector3f& relativePosition = Vector3f());

    static void spawnHitEffects(Entity* effectEntity, Entity* hitTarget);
};

NS_MG_END
