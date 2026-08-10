#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/ecs/Types.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class PendingHitInfo : public Object
{
public:
    typedef Object Super;

public:
    PendingHitInfo() {}
    virtual ~PendingHitInfo() {}

    EntityId attackerId = INVALID_ENTITY_ID;
    HitType hitType     = HitType::kHitNone;  // 运行时严重度（用于优先级）
    int32_t tableHitType = 0;                 // skill_hit.hit_type 原值 0/1/2
    int32_t hitId        = 0;
    int32_t displacementId = -1;
    std::string hitState;
    int32_t hitstunMs = 0;
    float impulseX    = 0.0f;
    float impulseZ    = 0.0f;

    // Phase 1.4
    float damage              = 0.0f;
    bool isCrit               = false;
    bool isDodge              = false;
    bool hitMust              = false;
    int32_t hurtType          = 0;
    int32_t hitRigidity       = 0;
    int32_t freezeTimeMs      = 0;
    int32_t freezeDelayMs     = 0;
    int32_t freezeControlRole = 0;
    int32_t freezeControlEffect = 0;
    EntityId effectEntityId   = INVALID_ENTITY_ID;

    MG_DEFINE_SERIALIZABLE(attackerId,
                           hitType,
                           tableHitType,
                           hitId,
                           displacementId,
                           hitState,
                           hitstunMs,
                           impulseX,
                           impulseZ,
                           damage,
                           isCrit,
                           isDodge,
                           hitMust,
                           hurtType,
                           hitRigidity,
                           freezeTimeMs,
                           freezeDelayMs,
                           freezeControlRole,
                           freezeControlEffect,
                           effectEntityId)
};

class HitReactComponent : public Component
{
public:
    typedef Component Super;

public:
    HitReactComponent() {}
    virtual ~HitReactComponent() {}

public:
    std::vector<PendingHitInfo> pendingHits;
    int32_t activeHitstunMs     = 0;
    HitType activeHitType       = HitType::kHitNone;
    int32_t activeTableHitType  = 0;

    MG_DEFINE_SERIALIZABLE(pendingHits, activeHitstunMs, activeHitType, activeTableHitType)
};

NS_MG_END
