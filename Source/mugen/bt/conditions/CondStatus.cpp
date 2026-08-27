#include "mugen/bt/conditions/CondStatus.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/Components.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/StdC.h"
#include "mugen/skill/SkillManager.h"

NS_MG_BEGIN

CondStatus::CondStatus() {}

CondStatus::CondStatus(BehaviorKind kind) : kind(kind) {}

CondStatus::~CondStatus() {}

namespace
{
bool isCasting(const BTContext& ctx)
{
    if (auto* mgr = SkillManager::of(ctx.entity))
    {
        if (mgr->activeSkillAttackId > 0)
            return true;
    }
    if (auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent))
    {
        if ((behavior->statusTags & StateTag::kTagAttackState) != 0)
            return true;
    }
    return false;
}

bool isHitOrDown(const BehaviorComponent* b)
{
    return b && (b->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)) != 0;
}

bool isStunned(const BTContext& ctx)
{
    if (auto* mgr = BuffManager::of(ctx.entity))
        return mgr->stunRef > 0;
    return false;
}

bool isStaticLocked(const BehaviorComponent* b)
{
    return b && b->staticRemainMs > 0;
}
}  // namespace

bool CondStatus::check(BTContext& ctx)
{
    auto* b = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (!b)
        return false;
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* input     = MG_GET_COMPONENT(ctx.entity, InputComponent);
    auto* mgr       = SkillManager::of(ctx.entity);

    switch (kind)
    {
    case BehaviorKind::kDeath:
        if (!attribute || attribute->hp > 0.0f || b->reviveRequested)
            return false;
        if (b->currentKind == static_cast<int32_t>(BehaviorKind::kDeath))
            return true;
        switch (static_cast<BehaviorKind>(b->currentKind))
        {
        case BehaviorKind::kStun:
        case BehaviorKind::kHitUp:
        case BehaviorKind::kHitDown:
        case BehaviorKind::kHitFloor:
        case BehaviorKind::kHitSwitch:
        case BehaviorKind::kGetUp:
            return false;
        default:
            return true;
        }

    case BehaviorKind::kRevive:
        return b->reviveRequested;

    case BehaviorKind::kWake:
        return b->wakeRemainMs > 0 || b->currentKind == static_cast<int32_t>(BehaviorKind::kWake);

    case BehaviorKind::kGetUp:
        return b->currentKind == static_cast<int32_t>(BehaviorKind::kGetUp);

    case BehaviorKind::kHitFloor:
        return (b->statusTags & StateTag::kTagDownState) && b->getUpRemainMs <= 0 &&
               b->currentKind == static_cast<int32_t>(BehaviorKind::kHitFloor);

    case BehaviorKind::kHitDown:
        return (b->statusTags & StateTag::kTagHitState) &&
               b->currentKind == static_cast<int32_t>(BehaviorKind::kHitDown);

    case BehaviorKind::kHitUp:
        return (b->statusTags & StateTag::kTagHitState) && b->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp);

    case BehaviorKind::kHitSwitch:
        return (b->statusTags & StateTag::kTagHitState) &&
               b->currentKind == static_cast<int32_t>(BehaviorKind::kHitSwitch);

    case BehaviorKind::kStun:
        return (b->statusTags & StateTag::kTagHitState) && !(b->statusTags & StateTag::kTagDownState) &&
               b->currentKind == static_cast<int32_t>(BehaviorKind::kStun);

    case BehaviorKind::kAttack:
        return mgr && mgr->activeSkillAttackId > 0;

    case BehaviorKind::kDash:
        if (isCasting(ctx) || isHitOrDown(b) || isStunned(ctx) || isStaticLocked(b))
            return false;
        return (b->statusTags & StateTag::kTagDashState) != 0;

    case BehaviorKind::kWalk:
        if (isCasting(ctx) || isHitOrDown(b) || isStunned(ctx) || isStaticLocked(b))
            return false;
        if (b->statusTags & StateTag::kTagDashState)
            return false;
        return b->clickToWalk && bt_util::anyMoveKeyDown(input);

    case BehaviorKind::kIdle:
        if (attribute && attribute->hp <= 0.0f)
            return false;
        if (isCasting(ctx) || isHitOrDown(b) || isStunned(ctx) || isStaticLocked(b))
            return false;
        if (b->reviveRequested || b->wakeRemainMs > 0 || b->currentKind == static_cast<int32_t>(BehaviorKind::kWake))
            return false;
        if (b->statusTags & StateTag::kTagDashState)
            return false;
        if (b->clickToWalk && bt_util::anyMoveKeyDown(input))
            return false;
        return true;

    default:
        return false;
    }
}

void CondStatus::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(static_cast<int32_t>(kind));
}

bool CondStatus::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    int32_t v = 0;
    if (!byteBuffer.getInt32(v))
        return false;
    kind = static_cast<BehaviorKind>(v);
    return true;
}

NS_MG_END
