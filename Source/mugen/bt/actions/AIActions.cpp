#include "mugen/bt/actions/AIActions.h"

#include "mugen/Components.h"
#include "mugen/ai/AiAgent.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

PatrolAction::PatrolAction() {}

PatrolAction::~PatrolAction() {}

AlertAction::AlertAction() {}

AlertAction::~AlertAction() {}

ChaseAction::ChaseAction() {}

ChaseAction::~ChaseAction() {}

JostledAction::JostledAction() {}

JostledAction::~JostledAction() {}

PathFindingAction::PathFindingAction() {}

PathFindingAction::~PathFindingAction() {}

namespace
{

// 面向目标 X
void faceToward(TransformComponent* self, int targetX)
{
    if (!self)
        return;
    if (targetX > self->position.x)
        self->facingDirection = FacingDirection::kFacingRight;
    else if (targetX < self->position.x)
        self->facingDirection = FacingDirection::kFacingLeft;
}

// 把位移折成 -1/0/1 的 moveDir，供本帧注入 Input
void setMoveToward(AiAgent* agent, int fromX, int fromY, int toX, int toY)
{
    if (!agent)
        return;
    const int dx    = toX - fromX;
    const int dy    = toY - fromY;
    agent->moveDirX = 0;
    agent->moveDirY = 0;
    if (std::abs(dx) >= std::abs(dy))
    {
        if (dx > 2)
            agent->moveDirX = 1;
        else if (dx < -2)
            agent->moveDirX = -1;
    }
    if (std::abs(dy) > 2)
    {
        if (dy > 2)
            agent->moveDirY = 1;
        else if (dy < -2)
            agent->moveDirY = -1;
    }
    if (agent->moveDirX == 0 && std::abs(dx) > 2)
        agent->moveDirX = dx > 0 ? 1 : -1;
}

// 先对齐 X 再对齐 Y，沿可行走矩形回出生点
void setMoveManhattan(AiAgent* agent, int fromX, int fromY, int toX, int toY)
{
    if (!agent)
        return;
    agent->moveDirX = 0;
    agent->moveDirY = 0;
    const int dx    = toX - fromX;
    const int dy    = toY - fromY;
    if (std::abs(dx) > 2)
        agent->moveDirX = dx > 0 ? 1 : -1;
    else if (std::abs(dy) > 2)
        agent->moveDirY = dy > 0 ? 1 : -1;
}

void applyAiMoveToInput(BTContext& ctx, AiAgent* agent)
{
    auto* input = MG_GET_COMPONENT(ctx.entity, InputComponent);
    if (!input || !agent)
        return;
    auto setMove = [&](int32_t slot, bool down) {
        if (down)
            MG_BIT_SET(input->keyDown, 1u << slot);
        else
            MG_BIT_REMOVE(input->keyDown, 1u << slot);
    };
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT), agent->moveDirX < 0);
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT), agent->moveDirX > 0);
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_UP), agent->moveDirY > 0);
    setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN), agent->moveDirY < 0);
    bt_util::applyLocomotionVelocity(ctx);
}

// 巡逻/追击/寻路与 Walk 共用 running，停步用 stand。模板里没有 Patrol/Chase 分支。
void applyAiLocomotion(BTContext& ctx, AiAgent* agent, bool moving)
{
    if (auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent))
        behavior->statusTags &= ~StateTag::kTagDashState;
    applyAiMoveToInput(ctx, agent);
    bt_util::setBranchKind(ctx, moving ? BehaviorKind::kWalk : BehaviorKind::kIdle);
    if (auto* avatar = MG_GET_COMPONENT(ctx.entity, AvatarComponent))
        avatar->animationSpeed = moving ? 1.2f : 1.0f;
}

// 在出生点半径内随机下一个巡逻点
void pickPatrolTarget(AiAgent* agent, Random& rng)
{
    if (!agent || agent->patrolScope <= 0)
        return;
    const int32_t r      = agent->patrolScope;
    const int32_t sx     = static_cast<int32_t>(std::lround(agent->spawnPosition.x));
    const int32_t sy     = static_cast<int32_t>(std::lround(agent->spawnPosition.y));
    agent->patrolTargetX = sx + rng.nextInt(-r, r);
    agent->patrolTargetY = sy + rng.nextInt(-r, r);
    agent->patrolState   = 1;
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
    auto* agent = AiAgent::of(ctx.entity);
    if (!agent)
        return;
    if (agent->patrolState == 0 || (agent->patrolTargetX == 0 && agent->patrolTargetY == 0 && agent->patrolState != 2))
        pickPatrolTarget(agent, AiAgent::worldRandom(ctx.entity->getECSManager()));
}

BTStatus PatrolAction::onActionUpdate(BTContext& ctx, int32_t dtMs)
{
    auto* agent     = AiAgent::of(ctx.entity);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (!agent || !transform)
    {
        return BTStatus::Failure;
    }

    if (agent->patrolWaitRemainMs > 0)
    {
        agent->patrolWaitRemainMs = (std::max)(0, agent->patrolWaitRemainMs - dtMs);
        agent->moveDirX           = 0;
        agent->moveDirY           = 0;
        agent->patrolState        = 2;
        applyAiLocomotion(ctx, agent, false);
        if (agent->patrolWaitRemainMs <= 0)
            pickPatrolTarget(agent, AiAgent::worldRandom(ctx.entity->getECSManager()));
        return BTStatus::Running;
    }

    const int dx = agent->patrolTargetX - transform->position.x;
    const int dy = agent->patrolTargetY - transform->position.y;
    if (dx * dx + dy * dy < 100)
    {
        agent->moveDirX           = 0;
        agent->moveDirY           = 0;
        agent->patrolState        = 2;
        agent->patrolWaitRemainMs = AiAgent::worldRandom(ctx.entity->getECSManager()).nextInt(800, 1800);
        applyAiLocomotion(ctx, agent, false);
        return BTStatus::Running;
    }

    setMoveToward(agent, transform->position.x, transform->position.y, agent->patrolTargetX, agent->patrolTargetY);
    faceToward(transform, agent->patrolTargetX);
    applyAiLocomotion(ctx, agent, true);
    return BTStatus::Running;
}

void AlertAction::onActionEnter(BTContext& ctx)
{
    auto* agent = AiAgent::of(ctx.entity);
    if (agent && agent->alertRemainMs <= 0)
    {
        const AiConfig* ai = agent->config();
        int32_t lo         = ai ? ai->alertDelayTime.x : 0;
        int32_t hi         = ai ? ai->alertDelayTime.y : 0;
        if (hi < lo)
            std::swap(lo, hi);
        if (lo <= 0 && hi <= 0)
            agent->alertRemainMs = 0;
        else
            agent->alertRemainMs = AiAgent::worldRandom(ctx.entity->getECSManager()).nextInt(lo, hi);
    }
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (auto* player = AiAgent::findNearestPlayer(ctx.entity->getECSManager(), transform))
    {
        if (auto* ptf = MG_GET_COMPONENT(player, TransformComponent))
            faceToward(transform, ptf->position.x);
    }
    if (agent)
    {
        agent->moveDirX = 0;
        agent->moveDirY = 0;
        applyAiLocomotion(ctx, agent, false);
    }
}

BTStatus AlertAction::onActionUpdate(BTContext& ctx, int32_t dtMs)
{
    auto* agent = AiAgent::of(ctx.entity);
    if (!agent)
    {
        return BTStatus::Failure;
    }
    agent->moveDirX = 0;
    agent->moveDirY = 0;
    applyAiLocomotion(ctx, agent, false);
    if (agent->alertRemainMs > 0)
        agent->alertRemainMs = (std::max)(0, agent->alertRemainMs - dtMs);
    if (agent->alertRemainMs <= 0)
    {
        agent->alertDone = true;
        return BTStatus::Success;
    }
    return BTStatus::Running;
}

void ChaseAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kWalk);
}

BTStatus ChaseAction::onActionUpdate(BTContext& ctx, int32_t /*dtMs*/)
{
    auto* agent     = AiAgent::of(ctx.entity);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (!agent || !transform)
    {
        return BTStatus::Failure;
    }

    Entity* player = AiAgent::findNearestPlayer(ctx.entity->getECSManager(), transform);
    if (!player)
    {
        return BTStatus::Failure;
    }
    auto* ptf = MG_GET_COMPONENT(player, TransformComponent);
    if (!ptf)
    {
        return BTStatus::Failure;
    }

    faceToward(transform, ptf->position.x);
    setMoveToward(agent, transform->position.x, transform->position.y, ptf->position.x, ptf->position.y);
    applyAiLocomotion(ctx, agent, true);
    return BTStatus::Running;
}

void JostledAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kWalk);
}

BTStatus JostledAction::onActionUpdate(BTContext& ctx, int32_t /*dtMs*/)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kWalk);
    auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    if (!physics)
    {
        return BTStatus::Failure;
    }

    auto* ecs = ctx.entity->getECSManager();
    Signature sig;
    sig.set(ecs->getComponentTypeId("PhysicsComponent"));
    sig.set(ecs->getComponentTypeId("IdentityComponent"));

    Entity* other        = nullptr;
    PhysicsComponent* op = nullptr;
    for (Entity* e : ecs->getEntitiesBySignature(sig))
    {
        if (!e || e == ctx.entity)
            continue;
        auto* oid = MG_GET_COMPONENT(e, IdentityComponent);
        if (!oid || (oid->category != EntityCategory::kPlayer && oid->category != EntityCategory::kMonster))
            continue;
        auto* p = MG_GET_COMPONENT(e, PhysicsComponent);
        if (aabbOverlap(physics, p))
        {
            other = e;
            op    = p;
            break;
        }
    }
    if (!other || !op)
    {
        return BTStatus::Success;
    }

    // 仅由较小 entity id 施加冲量，避免双倍推开
    if (ctx.entity->getId() > other->getId())
    {
        return BTStatus::Running;
    }

    float dx        = physics->position.x - op->position.x;
    float dy        = physics->position.y - op->position.y;
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
    physics->impulseVelocity.x += dx * kPush;
    physics->impulseVelocity.y += dy * kPush;
    op->impulseVelocity.x -= dx * kPush;
    op->impulseVelocity.y -= dy * kPush;
    return BTStatus::Running;
}

void PathFindingAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kWalk);
    if (auto* agent = AiAgent::of(ctx.entity))
        agent->pathFindingActive = true;
}

BTStatus PathFindingAction::onActionUpdate(BTContext& ctx, int32_t /*dtMs*/)
{
    auto* agent     = AiAgent::of(ctx.entity);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (!agent || !transform)
    {
        return BTStatus::Failure;
    }
    const int sx     = static_cast<int>(std::lround(agent->spawnPosition.x));
    const int sy     = static_cast<int>(std::lround(agent->spawnPosition.y));
    const int dx     = sx - transform->position.x;
    const int dy     = sy - transform->position.y;
    const int arrive = (std::max)(32, agent->patrolScope);
    if (std::abs(dx) <= arrive && std::abs(dy) <= arrive)
    {
        agent->moveDirX          = 0;
        agent->moveDirY          = 0;
        agent->pathFindingActive = false;
        applyAiLocomotion(ctx, agent, false);
        return BTStatus::Success;
    }
    setMoveManhattan(agent, transform->position.x, transform->position.y, sx, sy);
    faceToward(transform, sx);
    applyAiLocomotion(ctx, agent, true);
    return BTStatus::Running;
}

NS_MG_END
