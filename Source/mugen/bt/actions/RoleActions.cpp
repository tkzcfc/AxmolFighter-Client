#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/Components.h"

NS_MG_BEGIN

void LocomoAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, kind);
}

BTStatus LocomoAction::onActionTick(BTContext& ctx, int32_t /*dtMs*/)
{
    // 空中可在 Walk/Dash/Idle 间切换动画
    if (ctx.behavior && (ctx.behavior->statusTags & StateTag::kTagAirborne))
    {
        if (bt_util::anyMoveKeyDown(ctx.input))
        {
            const auto want = (ctx.behavior->statusTags & StateTag::kTagDashState) ? BehaviorKind::kDash
                                                                                    : BehaviorKind::kWalk;
            if (kind == want || kind == BehaviorKind::kIdle)
                bt_util::setBranchKind(ctx, want);
        }
        else if (kind == BehaviorKind::kIdle || kind == BehaviorKind::kWalk || kind == BehaviorKind::kDash)
            bt_util::setBranchKind(ctx, BehaviorKind::kIdle);
    }
    else
    {
        bt_util::setBranchKind(ctx, kind);
    }

    if (kind == BehaviorKind::kWalk || kind == BehaviorKind::kDash || kind == BehaviorKind::kIdle)
        bt_util::applyLocomotionVelocity(ctx);

    return BTStatus::Running;
}

void HitReactAction::onActionEnter(BTContext& ctx)
{
    if (!ctx.behavior)
        return;
    bt_util::playBranchAnim(ctx.behavior, ctx.avatar);
    if (ctx.bt)
        ctx.bt->activeBranchKind = ctx.behavior->currentKind;
    if (ctx.physics)
    {
        ctx.physics->velocity.x = 0;
        ctx.physics->velocity.y = 0;
    }
}

BTStatus HitReactAction::onActionTick(BTContext& ctx, int32_t /*dtMs*/)
{
    // 受击恢复由 BehaviorTreeSystem 前置段 tickHitRecovery 驱动；此处只维持动画/停步
    if (ctx.physics)
    {
        ctx.physics->velocity.x = 0;
        ctx.physics->velocity.y = 0;
    }
    if (ctx.behavior)
        bt_util::playBranchAnim(ctx.behavior, ctx.avatar);
    return BTStatus::Running;
}

void DeathAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, BehaviorKind::kDeath);
    if (ctx.physics)
    {
        ctx.physics->velocity.x = 0;
        ctx.physics->velocity.y = 0;
    }
}

BTStatus DeathAction::onActionTick(BTContext& ctx, int32_t /*dtMs*/)
{
    if (ctx.physics)
    {
        ctx.physics->velocity.x = 0;
        ctx.physics->velocity.y = 0;
    }
    return BTStatus::Running;
}

void HoldAttackAction::onActionEnter(BTContext& ctx)
{
    if (ctx.behavior)
    {
        ctx.behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kAttack);
        ctx.behavior->currentBranchIndex = -1;
    }
    if (ctx.bt)
        ctx.bt->activeBranchKind = static_cast<int32_t>(BehaviorKind::kAttack);
}

BTStatus HoldAttackAction::onActionTick(BTContext& /*ctx*/, int32_t /*dtMs*/)
{
    return BTStatus::Running;
}

NS_MG_END
