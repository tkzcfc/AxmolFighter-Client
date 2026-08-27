#pragma once

#include "mugen/core/Object.h"
#include "mugen/core/ecs/Types.h"
#include "mugen/core/math/Vec3.h"

#include <vector>

NS_MG_BEGIN

class Entity;
class EffectConfig;
class TransformComponent;
class Random;

// 特效实体运行时：寿命 / 弹道 / 命中会话活在对象上。无 EffectManager（一实体一个 Effect）。
class Effect : public Object
{
public:
    typedef Object Super;

public:
    Effect() {}

    virtual ~Effect() {}

    const char* typeName() const { return "Effect"; }

    static Effect* of(Entity* entity);

    void bindFromConfig(const EffectConfig* cfg,
                        EntityId owner,
                        int32_t skillHitLookupId,
                        bool chainFromParent,
                        const TransformComponent* originTf);

    void bindVisual(EntityId owner, bool followOwner, int32_t lifeMs, const Vector3f& rel);

    bool isAlive() const;

    bool update(Entity* entity, int32_t dtMs);

    void tryHit(Entity* entity, int32_t dtMs, Random& rng, const std::vector<Entity*>& defenders);

public:
    int32_t effectId   = 0;
    int32_t skillHitId = 0;
    EntityId ownerId   = INVALID_ENTITY_ID;
    int32_t lifetimeMs = 500;
    int32_t elapsedMs  = 0;
    bool follow        = false;
    float radius       = 40.0f;
    Vector3f relativePosition;
    float moveVx          = 0.0f;
    float moveVy          = 0.0f;
    bool chainSpawned     = false;
    int32_t hitCount      = 0;
    int32_t hitCooldownMs = 0;
    std::vector<uint32_t> hitEntityIds;
    int32_t baseSkillId   = 0;
    int32_t slotIndex     = 0;
    int32_t autoRelease   = 0;
    int32_t followMode    = 0;
    bool allowNextEffect  = true;
    bool destroyRequested = false;

public:
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
                           hitEntityIds,
                           baseSkillId,
                           slotIndex,
                           autoRelease,
                           followMode,
                           allowNextEffect,
                           destroyRequested)
};

NS_MG_END
