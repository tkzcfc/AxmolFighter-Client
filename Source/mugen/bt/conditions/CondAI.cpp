#include "mugen/bt/conditions/CondAI.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"

#include <cmath>

NS_MG_BEGIN

namespace
{

bool isCombatBlocked(const BehaviorComponent* b, const AttributeComponent* attr)
{
    if (attr && attr->currentAttribute.hp <= 0.0f)
        return true;
    if (!b)
        return true;
    if (b->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
        return true;
    return false;
}

Entity* findNearestPlayerEntity(ECSManager* ecs, const TransformComponent* selfTf)
{
    if (!ecs || !selfTf)
        return nullptr;
    Entity* best     = nullptr;
    int64_t bestDist = 0;
    Signature sig;
    sig.set(ecs->getComponentTypeId("IdentityComponent"));
    sig.set(ecs->getComponentTypeId("TransformComponent"));
    for (Entity* e : ecs->getEntitiesBySignature(sig))
    {
        auto* id = MG_GET_COMPONENT(e, IdentityComponent);
        auto* tf = MG_GET_COMPONENT(e, TransformComponent);
        if (!id || !tf || id->category != EntityCategory::kPlayer)
            continue;
        if (auto* attr = MG_GET_COMPONENT(e, AttributeComponent))
        {
            if (attr->currentAttribute.hp <= 0.0f)
                continue;
        }
        const int64_t dx   = static_cast<int64_t>(tf->position.x) - selfTf->position.x;
        const int64_t dy   = static_cast<int64_t>(tf->position.y) - selfTf->position.y;
        const int64_t dist = dx * dx + dy * dy;
        if (!best || dist < bestDist)
        {
            best     = e;
            bestDist = dist;
        }
    }
    return best;
}

const AiConfig* resolveAiConfig(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!behavior || !behavior->roleConfig || behavior->roleConfig->aiIds.empty())
        return nullptr;
    return Config::getInstance()->getAiConfigById(behavior->roleConfig->aiIds.front());
}

AIComponent* ensureAi(Entity* entity)
{
    return entity ? MG_GET_COMPONENT(entity, AIComponent) : nullptr;
}

bool inScopeXZ(const TransformComponent* self,
               const TransformComponent* other,
               const Vector2i& scopeX,
               const Vector2i& scopeZ)
{
    if (!self || !other)
        return false;
    const int dx = std::abs(self->position.x - other->position.x);
    const int dz = std::abs(self->position.y - other->position.y);
    const int minX = scopeX.x;
    const int maxX = scopeX.y > 0 ? scopeX.y : 2000;
    const int minZ = scopeZ.x;
    const int maxZ = scopeZ.y > 0 ? scopeZ.y : 2000;
    return dx >= minX && dx <= maxX && dz >= minZ && dz <= maxZ;
}

bool inMaxScope(const TransformComponent* self, const TransformComponent* other, int maxX, int maxZ)
{
    if (!self || !other)
        return false;
    const int dx = std::abs(self->position.x - other->position.x);
    const int dz = std::abs(self->position.y - other->position.y);
    return dx <= maxX && dz <= maxZ;
}

int32_t attackRangeX(Entity* self)
{
    auto* aiComp = MG_GET_COMPONENT(self, AIComponent);
    if (aiComp && aiComp->aiConfigId > 0)
    {
        const auto* ai = Config::getInstance()->getAiConfigById(aiComp->aiConfigId);
        if (ai && !ai->skillAiIds.empty())
        {
            for (int32_t sid : ai->skillAiIds)
            {
                if (const auto* cfg = Config::getInstance()->getSkillAiConfigById(sid))
                {
                    if (cfg->oppDisX.y > 0)
                        return cfg->oppDisX.y;
                }
            }
        }
    }
    return 200;
}

bool aabbOverlap(const PhysicsComponent* a, const PhysicsComponent* b)
{
    if (!a || !b || a->isStaticBody || b->isStaticBody)
        return false;
    const float ax0 = a->position.x - a->size.x * 0.5f;
    const float ax1 = a->position.x + a->size.x * 0.5f;
    const float ay0 = a->position.y - a->size.y * 0.5f;
    const float ay1 = a->position.y + a->size.y * 0.5f;
    const float bx0 = b->position.x - b->size.x * 0.5f;
    const float bx1 = b->position.x + b->size.x * 0.5f;
    const float by0 = b->position.y - b->size.y * 0.5f;
    const float by1 = b->position.y + b->size.y * 0.5f;
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
}

}  // namespace

bool CondPatrol::check(BTContext& ctx)
{
    if (!ctx.entity || isCombatBlocked(ctx.behavior, ctx.attribute))
        return false;
    if (ctx.skillCast && ctx.skillCast->activeSkillAttackId > 0)
        return false;

    auto* ai = ensureAi(ctx.entity);
    if (!ai || ai->patrolScope <= 0)
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    const AiConfig* cfg = resolveAiConfig(ctx.entity);
    Entity* player      = findNearestPlayerEntity(ctx.ecs, ctx.transform);
    if (player && cfg)
    {
        auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
        // 玩家进入目标范围则不再巡逻
        if (ptf && inScopeXZ(ctx.transform, ptf, cfg->targetScopeX, cfg->targetScopeZ))
            return false;
        // 或已在追击范围内
        if (ptf)
        {
            const int maxX = cfg->chaseScopeX.y > 0 ? cfg->chaseScopeX.y : 2000;
            const int maxZ = cfg->chaseScopeZ.y > 0 ? cfg->chaseScopeZ.y : 2000;
            if (inMaxScope(ctx.transform, ptf, maxX, maxZ))
                return false;
        }
    }
    return true;
}

bool CondAlert::check(BTContext& ctx)
{
    if (!ctx.entity || isCombatBlocked(ctx.behavior, ctx.attribute))
        return false;
    if (ctx.skillCast && ctx.skillCast->activeSkillAttackId > 0)
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    const AiConfig* cfg = resolveAiConfig(ctx.entity);
    if (!cfg)
        return false;

    Entity* player = findNearestPlayerEntity(ctx.ecs, ctx.transform);
    auto* ai       = ensureAi(ctx.entity);
    if (!player)
    {
        if (ai)
        {
            ai->alertDone     = false;
            ai->alertRemainMs = 0;
        }
        return false;
    }
    auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
    if (!ptf)
        return false;

    if (!inScopeXZ(ctx.transform, ptf, cfg->targetScopeX, cfg->targetScopeZ))
    {
        if (ai)
        {
            ai->alertDone     = false;
            ai->alertRemainMs = 0;
        }
        return false;
    }

    if (!ai || ai->alertDone)
        return false;
    return true;
}

bool CondChase::check(BTContext& ctx)
{
    if (!ctx.entity || isCombatBlocked(ctx.behavior, ctx.attribute))
        return false;
    if (ctx.skillCast && ctx.skillCast->activeSkillAttackId > 0)
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    const AiConfig* cfg = resolveAiConfig(ctx.entity);
    if (!cfg)
        return false;

    Entity* player = findNearestPlayerEntity(ctx.ecs, ctx.transform);
    if (!player)
        return false;
    auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
    if (!ptf)
        return false;

    auto* ai = ensureAi(ctx.entity);
    const int maxX = cfg->chaseScopeX.y > 0 ? cfg->chaseScopeX.y : 2000;
    const int maxZ = cfg->chaseScopeZ.y > 0 ? cfg->chaseScopeZ.y : 2000;
    const bool inChase  = inMaxScope(ctx.transform, ptf, maxX, maxZ);
    const bool inTarget = inScopeXZ(ctx.transform, ptf, cfg->targetScopeX, cfg->targetScopeZ);
    // 警觉完成后，目标范围内也可追击；否则需在 chaseScope 内
    if (!(inChase || (ai && ai->alertDone && inTarget)))
        return false;

    const int atkR = attackRangeX(ctx.entity);
    const int dx   = std::abs(ctx.transform->position.x - ptf->position.x);
    const int dz   = std::abs(ctx.transform->position.y - ptf->position.y);
    if (dx <= atkR && dz <= atkR)
        return false;

    return true;
}

bool CondJostled::check(BTContext& ctx)
{
    if (!ctx.entity || !ctx.physics || isCombatBlocked(ctx.behavior, ctx.attribute))
        return false;
    if (ctx.skillCast && ctx.skillCast->activeSkillAttackId > 0)
        return false;
    if (ctx.physics->isStaticBody)
        return false;

    Signature sig;
    sig.set(ctx.ecs->getComponentTypeId("PhysicsComponent"));
    sig.set(ctx.ecs->getComponentTypeId("IdentityComponent"));
    for (Entity* other : ctx.ecs->getEntitiesBySignature(sig))
    {
        if (!other || other == ctx.entity)
            continue;
        auto* oid = MG_GET_COMPONENT(other, IdentityComponent);
        if (!oid || (oid->category != EntityCategory::kPlayer && oid->category != EntityCategory::kMonster))
            continue;
        auto* op = MG_GET_COMPONENT(other, PhysicsComponent);
        if (aabbOverlap(ctx.physics, op))
            return true;
    }
    return false;
}

NS_MG_END
