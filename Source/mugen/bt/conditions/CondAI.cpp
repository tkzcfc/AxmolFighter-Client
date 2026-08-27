#include "mugen/bt/conditions/CondAI.h"

#include "mugen/Components.h"
#include "mugen/ai/AiAgent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/skill/SkillManager.h"

#include <cmath>

NS_MG_BEGIN

CondPatrol::CondPatrol() {}

CondPatrol::~CondPatrol() {}

CondAlert::CondAlert() {}

CondAlert::~CondAlert() {}

CondChase::CondChase() {}

CondChase::~CondChase() {}

CondJostled::CondJostled() {}

CondJostled::~CondJostled() {}

CondPathFinding::CondPathFinding() {}

CondPathFinding::~CondPathFinding() {}

namespace
{

bool isCombatBlocked(const BehaviorComponent* b, const AttributeComponent* attr)
{
    if (attr && attr->hp <= 0.0f)
        return true;
    if (!b)
        return true;
    if (b->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
        return true;
    return false;
}

bool inScopeXZ(const TransformComponent* self,
               const TransformComponent* other,
               const Vector2i& scopeX,
               const Vector2i& scopeZ)
{
    if (!self || !other)
        return false;
    const int dx   = std::abs(self->position.x - other->position.x);
    const int dz   = std::abs(self->position.y - other->position.y);
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

// 取 skill_ai 的 oppDisX 上界作为停追距离；没有则 200
int32_t attackRangeX(Entity* self)
{
    auto* agent = AiAgent::of(self);
    if (agent && agent->aiConfigId > 0)
    {
        const auto* ai = Config::getInstance()->getAiConfigById(agent->aiConfigId);
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

bool isCasting(Entity* entity)
{
    auto* mgr = SkillManager::of(entity);
    return mgr && mgr->activeSkillAttackId > 0;
}

}  // namespace

bool CondPatrol::check(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (isCombatBlocked(behavior, attribute))
        return false;
    if (isCasting(ctx.entity))
        return false;

    auto* agent = AiAgent::of(ctx.entity);
    if (!agent || agent->patrolScope <= 0)
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    const AiConfig* cfg = AiAgent::resolveConfig(ctx.entity);
    Entity* player      = AiAgent::findNearestPlayer(ctx.entity->getECSManager(), transform, true);
    if (player && cfg)
    {
        auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
        if (ptf && inScopeXZ(transform, ptf, cfg->targetScopeX, cfg->targetScopeZ))
            return false;
        if (ptf)
        {
            const int maxX = cfg->chaseScopeX.y > 0 ? cfg->chaseScopeX.y : 2000;
            const int maxZ = cfg->chaseScopeZ.y > 0 ? cfg->chaseScopeZ.y : 2000;
            if (inMaxScope(transform, ptf, maxX, maxZ))
                return false;
        }
    }
    return true;
}

bool CondAlert::check(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (isCombatBlocked(behavior, attribute))
        return false;
    if (isCasting(ctx.entity))
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    const AiConfig* cfg = AiAgent::resolveConfig(ctx.entity);
    if (!cfg)
        return false;

    Entity* player = AiAgent::findNearestPlayer(ctx.entity->getECSManager(), transform, true);
    auto* agent    = AiAgent::of(ctx.entity);
    if (!player)
    {
        if (agent)
        {
            agent->alertDone     = false;
            agent->alertRemainMs = 0;
        }
        return false;
    }
    auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
    if (!ptf)
        return false;

    if (!inScopeXZ(transform, ptf, cfg->targetScopeX, cfg->targetScopeZ))
    {
        if (agent)
        {
            agent->alertDone     = false;
            agent->alertRemainMs = 0;
        }
        return false;
    }

    if (!agent || agent->alertDone)
        return false;
    return true;
}

bool CondChase::check(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (isCombatBlocked(behavior, attribute))
        return false;
    if (isCasting(ctx.entity))
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    const AiConfig* cfg = AiAgent::resolveConfig(ctx.entity);
    if (!cfg)
        return false;

    Entity* player = AiAgent::findNearestPlayer(ctx.entity->getECSManager(), transform, true);
    if (!player)
        return false;
    auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
    if (!ptf)
        return false;

    auto* agent         = AiAgent::of(ctx.entity);
    const int maxX      = cfg->chaseScopeX.y > 0 ? cfg->chaseScopeX.y : 2000;
    const int maxZ      = cfg->chaseScopeZ.y > 0 ? cfg->chaseScopeZ.y : 2000;
    const bool inChase  = inMaxScope(transform, ptf, maxX, maxZ);
    const bool inTarget = inScopeXZ(transform, ptf, cfg->targetScopeX, cfg->targetScopeZ);
    if (!(inChase || (agent && agent->alertDone && inTarget)))
        return false;

    const int atkR = attackRangeX(ctx.entity);
    const int dx   = std::abs(transform->position.x - ptf->position.x);
    const int dz   = std::abs(transform->position.y - ptf->position.y);
    if (dx <= atkR && dz <= atkR)
        return false;

    return true;
}

bool CondJostled::check(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* physics   = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    if (!physics || isCombatBlocked(behavior, attribute))
        return false;
    if (isCasting(ctx.entity))
        return false;
    if (physics->isStaticBody)
        return false;

    auto* ecs = ctx.entity->getECSManager();
    Signature sig;
    sig.set(ecs->getComponentTypeId("PhysicsComponent"));
    sig.set(ecs->getComponentTypeId("IdentityComponent"));
    for (Entity* other : ecs->getEntitiesBySignature(sig))
    {
        if (!other || other == ctx.entity)
            continue;
        auto* oid = MG_GET_COMPONENT(other, IdentityComponent);
        if (!oid || (oid->category != EntityCategory::kPlayer && oid->category != EntityCategory::kMonster))
            continue;
        auto* op = MG_GET_COMPONENT(other, PhysicsComponent);
        if (aabbOverlap(physics, op))
            return true;
    }
    return false;
}

bool CondPathFinding::check(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (isCombatBlocked(behavior, attribute))
        return false;
    if (isCasting(ctx.entity))
        return false;

    auto* identity = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
    if (!identity || identity->category != EntityCategory::kMonster)
        return false;

    auto* agent = AiAgent::of(ctx.entity);
    if (!agent || agent->patrolScope <= 0 || !transform)
        return false;

    const int dx    = std::abs(transform->position.x - static_cast<int>(agent->spawnPosition.x));
    const int dy    = std::abs(transform->position.y - static_cast<int>(agent->spawnPosition.y));
    const int enter = agent->patrolScope * 2;
    const int leave = agent->patrolScope;
    if (dx > enter || dy > enter)
        return true;
    return agent->pathFindingActive && (dx > leave || dy > leave);
}

NS_MG_END
