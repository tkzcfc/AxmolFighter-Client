#include "mugen/bt/actions/AIActions.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"

#include <cmath>

NS_MG_BEGIN

namespace
{

Random& worldRng(ECSManager* ecs)
{
    static Random fallback;
    if (!ecs)
        return fallback;
    if (auto* word = reinterpret_cast<GameWord*>(ecs->getUserdata()))
        return word->random;
    return fallback;
}

Entity* findNearestPlayer(ECSManager* ecs, const TransformComponent* selfTf)
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

AIComponent* getAi(Entity* e)
{
    return e ? MG_GET_COMPONENT(e, AIComponent) : nullptr;
}

void faceToward(TransformComponent* self, int targetX)
{
    if (!self)
        return;
    if (targetX > self->position.x)
        self->facingDirection = FacingDirection::kFacingRight;
    else if (targetX < self->position.x)
        self->facingDirection = FacingDirection::kFacingLeft;
}

void setMoveToward(AIComponent* ai, int fromX, int fromY, int toX, int toY)
{
    if (!ai)
        return;
    const int dx = toX - fromX;
    const int dy = toY - fromY;
    ai->moveDirX = 0;
    ai->moveDirY = 0;
    if (std::abs(dx) >= std::abs(dy))
    {
        if (dx > 2)
            ai->moveDirX = 1;
        else if (dx < -2)
            ai->moveDirX = -1;
    }
    if (std::abs(dy) > 2)
    {
        if (dy > 2)
            ai->moveDirY = 1;
        else if (dy < -2)
            ai->moveDirY = -1;
    }
    if (ai->moveDirX == 0 && std::abs(dx) > 2)
        ai->moveDirX = dx > 0 ? 1 : -1;
}

void applyAiMoveToInput(BTContext& ctx, AIComponent* ai)
{
    if (!ctx.input || !ai)
        return;
    auto setMove = [&](int32_t slot, bool down) {
        if (down)
            MG_BIT_SET(ctx.input->keyDown, 1u << slot);
        else
            MG_BIT_REMOVE(ctx.input->keyDown, 1u << slot);
    };
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT), ai->moveDirX < 0);
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT), ai->moveDirX > 0);
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_UP), ai->moveDirY > 0);
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN), ai->moveDirY < 0);
    bt_util::applyLocomotionVelocity(ctx);
}

void pickPatrolTarget(AIComponent* ai, Random& rng)
{
    if (!ai || ai->patrolScope <= 0)
        return;
    const int32_t r = ai->patrolScope;
    const int32_t sx = static_cast<int32_t>(std::lround(ai->spawnPosition.x));
    const int32_t sy = static_cast<int32_t>(std::lround(ai->spawnPosition.y));
    ai->patrolTargetX = sx + rng.nextInt(-r, r);
    ai->patrolTargetY = sy + rng.nextInt(-r, r);
    ai->patrolState   = 1;
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

void PatrolAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kPatrol);
    auto* ai = getAi(ctx.entity);
    if (!ai)
        return;
    if (ai->patrolState == 0 || (ai->patrolTargetX == 0 && ai->patrolTargetY == 0 && ai->patrolState != 2))
        pickPatrolTarget(ai, worldRng(ctx.ecs));
}

BTStatus PatrolAction::onActionTick(BTContext& ctx, int32_t dtMs)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kPatrol);
    auto* ai = getAi(ctx.entity);
    if (!ai || !ctx.transform)
        return BTStatus::Failure;

    if (ai->patrolWaitRemainMs > 0)
    {
        ai->patrolWaitRemainMs = (std::max)(0, ai->patrolWaitRemainMs - dtMs);
        ai->moveDirX           = 0;
        ai->moveDirY           = 0;
        ai->patrolState        = 2;
        if (ai->patrolWaitRemainMs <= 0)
            pickPatrolTarget(ai, worldRng(ctx.ecs));
        return BTStatus::Running;
    }

    const int dx = ai->patrolTargetX - ctx.transform->position.x;
    const int dy = ai->patrolTargetY - ctx.transform->position.y;
    if (dx * dx + dy * dy < 100)
    {
        ai->moveDirX           = 0;
        ai->moveDirY           = 0;
        ai->patrolState        = 2;
        ai->patrolWaitRemainMs = worldRng(ctx.ecs).nextInt(800, 1800);
        return BTStatus::Running;
    }

    setMoveToward(ai, ctx.transform->position.x, ctx.transform->position.y, ai->patrolTargetX, ai->patrolTargetY);
    faceToward(ctx.transform, ai->patrolTargetX);
    if (ctx.behavior)
        ctx.behavior->statusTags &= ~StateTag::kTagDashState;
    applyAiMoveToInput(ctx, ai);
    return BTStatus::Running;
}

void AlertAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kAlert);
    auto* ai = getAi(ctx.entity);
    if (ai && ai->alertRemainMs <= 0)
        ai->alertRemainMs = 300;
    if (auto* player = findNearestPlayer(ctx.ecs, ctx.transform))
    {
        if (auto* ptf = MG_GET_COMPONENT(player, TransformComponent))
            faceToward(ctx.transform, ptf->position.x);
    }
    if (ai)
    {
        ai->moveDirX = 0;
        ai->moveDirY = 0;
    }
}

BTStatus AlertAction::onActionTick(BTContext& ctx, int32_t dtMs)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kAlert);
    auto* ai = getAi(ctx.entity);
    if (!ai)
        return BTStatus::Failure;
    ai->moveDirX = 0;
    ai->moveDirY = 0;
    if (ai->alertRemainMs > 0)
        ai->alertRemainMs = (std::max)(0, ai->alertRemainMs - dtMs);
    if (ai->alertRemainMs <= 0)
    {
        ai->alertDone = true;
        return BTStatus::Success;
    }
    return BTStatus::Running;
}

void ChaseAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kChase);
}

BTStatus ChaseAction::onActionTick(BTContext& ctx, int32_t /*dtMs*/)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kChase);
    auto* ai = getAi(ctx.entity);
    if (!ai || !ctx.transform)
        return BTStatus::Failure;

    Entity* player = findNearestPlayer(ctx.ecs, ctx.transform);
    if (!player)
        return BTStatus::Failure;
    auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
    if (!ptf)
        return BTStatus::Failure;

    faceToward(ctx.transform, ptf->position.x);
    setMoveToward(ai, ctx.transform->position.x, ctx.transform->position.y, ptf->position.x, ptf->position.y);
    if (ctx.behavior)
        ctx.behavior->statusTags |= StateTag::kTagDashState;
    applyAiMoveToInput(ctx, ai);
    return BTStatus::Running;
}

void JostledAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kJostled);
}

BTStatus JostledAction::onActionTick(BTContext& ctx, int32_t /*dtMs*/)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kJostled);
    if (!ctx.entity || !ctx.physics || !ctx.ecs)
        return BTStatus::Failure;

    Signature sig;
    sig.set(ctx.ecs->getComponentTypeId("PhysicsComponent"));
    sig.set(ctx.ecs->getComponentTypeId("IdentityComponent"));

    Entity* other = nullptr;
    PhysicsComponent* op = nullptr;
    for (Entity* e : ctx.ecs->getEntitiesBySignature(sig))
    {
        if (!e || e == ctx.entity)
            continue;
        auto* oid = MG_GET_COMPONENT(e, IdentityComponent);
        if (!oid || (oid->category != EntityCategory::kPlayer && oid->category != EntityCategory::kMonster))
            continue;
        auto* p = MG_GET_COMPONENT(e, PhysicsComponent);
        if (aabbOverlap(ctx.physics, p))
        {
            other = e;
            op    = p;
            break;
        }
    }
    if (!other || !op)
        return BTStatus::Success;

    // 仅由较小 entity id 施加冲量，避免双倍推开
    if (ctx.entity->getId() > other->getId())
        return BTStatus::Running;

    float dx = ctx.physics->position.x - op->position.x;
    float dy = ctx.physics->position.y - op->position.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f)
    {
        dx = 1.0f;
        dy = 0.0f;
    }
    else
    {
        dx /= len;
        dy /= len;
    }
    constexpr float kPush = 80.0f;
    ctx.physics->impulseVelocity.x += dx * kPush;
    // y 轴用 velocity.y 通道（项目物理 y 为平面纵深）
    ctx.physics->impulseVelocity.y += dy * kPush;
    op->impulseVelocity.x -= dx * kPush;
    op->impulseVelocity.y -= dy * kPush;
    return BTStatus::Running;
}

NS_MG_END
