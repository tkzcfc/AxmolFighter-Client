#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/ecs/Types.h"
#include "mugen/core/math/Vec3.h"

#include <vector>

NS_MG_BEGIN

// 运行时特效实体：携带攻击盒与 skill_hit 引用
class EffectLifetimeComponent : public Component
{
public:
    typedef Component Super;

    EffectLifetimeComponent() {}
    virtual ~EffectLifetimeComponent() {}

    int32_t effectId   = 0;
    int32_t skillHitId = 0;
    EntityId ownerId   = INVALID_ENTITY_ID;
    int32_t lifetimeMs = 500;
    int32_t elapsedMs  = 0;
    bool follow        = false;
    float radius       = 40.0f;
    Vector3f relativePosition;

    // 弹道（由 EffectConfig.velocity 在生成时推导）
    float moveVx      = 0.0f;
    float moveVy      = 0.0f;
    bool chainSpawned = false;

    // 多段打击
    int32_t hitCount     = 0;
    int32_t hitCooldownMs = 0;
    std::vector<uint32_t> hitEntityIds;

    MG_DEFINE_SERIALIZABLE(effectId,
                           skillHitId,
                           ownerId,
                           lifetimeMs,
                           elapsedMs,
                           follow,
                           radius,
                           relativePosition,
                           moveVx,
                           moveVy,
                           chainSpawned,
                           hitCount,
                           hitCooldownMs,
                           hitEntityIds);
};

NS_MG_END
