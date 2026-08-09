#include "mugen/bt/conditions/CondStatus.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/Components.h"
#include "mugen/bt/BtLocomotionUtils.h"

NS_MG_BEGIN

bool CondStatus::check(BTContext& ctx)
{
    auto* b = ctx.behavior;
    if (!b)
        return false;

    switch (kind)
    {
    case BehaviorKind::kDeath:
        return ctx.attribute && ctx.attribute->currentAttribute.hp <= 0.0f;

    case BehaviorKind::kGetUp:
        return (b->statusTags & StateTag::kTagDownState) && b->getUpRemainMs > 0;

    case BehaviorKind::kHitFloor:
        return (b->statusTags & StateTag::kTagDownState) && b->getUpRemainMs <= 0;

    case BehaviorKind::kHitUp:
        return (b->statusTags & StateTag::kTagHitState) &&
               b->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp);

    case BehaviorKind::kStun:
        return (b->statusTags & StateTag::kTagHitState) &&
               !(b->statusTags & StateTag::kTagDownState) &&
               b->currentKind != static_cast<int32_t>(BehaviorKind::kHitUp);

    case BehaviorKind::kAttack:
        return ctx.skillCast && ctx.skillCast->activeSkillAttackId > 0;

    case BehaviorKind::kDash:
        return (b->statusTags & StateTag::kTagMovable) &&
               (b->statusTags & StateTag::kTagDashState) &&
               bt_util::anyMoveKeyDown(ctx.input) &&
               !(b->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState));

    case BehaviorKind::kWalk:
        return (b->statusTags & StateTag::kTagMovable) &&
               !(b->statusTags & StateTag::kTagDashState) &&
               bt_util::anyMoveKeyDown(ctx.input) &&
               !(b->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState));

    case BehaviorKind::kIdle:
        // 兜底：非死亡即可
        return !(ctx.attribute && ctx.attribute->currentAttribute.hp <= 0.0f);

    default:
        return false;
    }
}

NS_MG_END
