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
    HitType hitType     = HitType::kHitNone;
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
    int32_t activeHitstunMs = 0;
    HitType activeHitType   = HitType::kHitNone;

    MG_DEFINE_SERIALIZABLE(pendingHits, activeHitstunMs, activeHitType)
};

NS_MG_END
