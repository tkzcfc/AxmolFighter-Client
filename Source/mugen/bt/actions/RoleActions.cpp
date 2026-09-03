#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/StdC.h"
#include "mugen/Components.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/conf/Config.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>

NS_MG_BEGIN

LocomoAction::LocomoAction() {}

LocomoAction::LocomoAction(BehaviorKind kind) : kind(kind) {}

LocomoAction::~LocomoAction() {}

HitKindAction::HitKindAction() {}

HitKindAction::HitKindAction(BehaviorKind kind) : kind(kind) {}

HitKindAction::~HitKindAction() {}

TimedKindAction::TimedKindAction() {}

TimedKindAction::TimedKindAction(BehaviorKind kind) : kind(kind) {}

TimedKindAction::~TimedKindAction() {}

DeathAction::DeathAction() {}

DeathAction::~DeathAction() {}

WakeAction::WakeAction() {}

WakeAction::~WakeAction() {}

ReviveAction::ReviveAction() {}

ReviveAction::~ReviveAction() {}

HoldAttackAction::HoldAttackAction() {}

HoldAttackAction::~HoldAttackAction() {}

namespace
{
bool readKind(ByteBuffer& byteBuffer, BehaviorKind& dest)
{
    int32_t v = 0;
    if (!byteBuffer.getInt32(v))
        return false;
    dest = static_cast<BehaviorKind>(v);
    return true;
}

bool isPlayer(Entity* entity)
{
    auto* id = MG_GET_COMPONENT(entity, IdentityComponent);
    return id && id->category == EntityCategory::kPlayer;
}

int32_t roleHitDispId(const BehaviorComponent* behavior)
{
    if (!behavior || !behavior->roleConfig)
        return -1;
    return behavior->roleConfig->hitDisplacementId;
}

void replayHitAnim(BTContext& ctx)
{
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* avatar   = MG_GET_COMPONENT(ctx.entity, AvatarComponent);
    if (!behavior)
        return;
    behavior->currentBranchIndex = -1;
    bt_util::playBranchAnim(behavior, avatar);
}

bool pollAnimEnd(Entity* entity, bool& animationEnd)
{
    if (animationEnd)
        return true;
    auto* avatar = MG_GET_COMPONENT(entity, AvatarComponent);
    if (!avatar || avatar->animationFinished || avatar->playback.isFinished())
        animationEnd = true;
    return animationEnd;
}

bool pollDispBrake(Entity* entity, bool& displacementEnd)
{
    auto* disp = MG_GET_COMPONENT(entity, DisplacementComponent);
    if (!disp || disp->finished || disp->activeId <= 0)
    {
        displacementEnd = true;
        return true;
    }
    if (disp->braked)
        displacementEnd = true;
    return displacementEnd;
}

bool pollDispAirOrLie(Entity* entity, bool& displacementEnd)
{
    auto* disp    = MG_GET_COMPONENT(entity, DisplacementComponent);
    auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent);
    if (disp && (disp->airEvent || disp->lieEvent))
        displacementEnd = true;
    if (physics && physics->onGround)
        displacementEnd = true;
    if (!disp || disp->finished || disp->activeId <= 0)
        displacementEnd = true;
    return displacementEnd;
}

bool pollDispLie(Entity* entity, bool& displacementEnd)
{
    auto* disp    = MG_GET_COMPONENT(entity, DisplacementComponent);
    auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent);
    if (disp && disp->lieEvent)
        displacementEnd = true;
    if (physics && physics->onGround)
        displacementEnd = true;
    if (!disp || disp->finished || disp->activeId <= 0)
        displacementEnd = true;
    return displacementEnd;
}

BTStatus finishToDeathOr(Entity* entity, BehaviorKind next)
{
    if (bt_util::isDeadHp(entity))
    {
        if (next == BehaviorKind::kGetUp || next == BehaviorKind::kWake || next == BehaviorKind::kHitSwitch ||
            next == BehaviorKind::kIdle)
        {
            bt_util::enterDeath(entity);
            return BTStatus::Success;
        }
    }
    bt_util::mixHitKind(entity, next, false);
    return BTStatus::Success;
}

}  // namespace

void LocomoAction::onActionEnter(BTContext& ctx)
{
    // 站立叶收招：清位移与力速度。走/跑叶不清，由攻击叶 exit 负责
    if (kind == BehaviorKind::kIdle)
    {
        auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
        if (auto* disp = MG_GET_COMPONENT(ctx.entity, DisplacementComponent))
        {
            disp->restoreGravity(physics);
            disp->reset();
        }
        if (physics)
        {
            physics->velocity.x = 0;
            physics->velocity.y = 0;
            physics->velocity.z = 0;
        }
    }
    bt_util::setBranchKind(ctx, kind);
}

BTStatus LocomoAction::onActionUpdate(BTContext& ctx, int32_t /*dtMs*/)
{
    bt_util::setBranchKind(ctx, kind);
    if (kind == BehaviorKind::kWalk || kind == BehaviorKind::kDash)
        bt_util::applyLocomotionVelocity(ctx);
    return BTStatus::Running;
}

void LocomoAction::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(static_cast<int32_t>(kind));
}

bool LocomoAction::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return readKind(byteBuffer, kind);
}

void HitKindAction::onActionEnter(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* hr        = MG_GET_COMPONENT(ctx.entity, HitReactComponent);
    animationEnd    = false;
    displacementEnd = false;
    afterEndMs      = 0;
    displacementId  = hr ? hr->activeDisplacementId : -1;

    if (kind == BehaviorKind::kStun)
    {
        const int32_t roleDisp = roleHitDispId(behavior);
        if (roleDisp == 0)
            displacementId = -1;
        bt_util::startDisplacementId(ctx.entity, displacementId, hr ? hr->knockbackFacing : 1.0f, true);
        if (auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent))
        {
            if (displacementId <= 0)
            {
                physics->velocity.x = 0;
                physics->velocity.y = 0;
            }
        }
    }
    else if (kind == BehaviorKind::kHitSwitch || kind == BehaviorKind::kGetUp)
    {
        if (auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent))
        {
            physics->velocity.x = 0;
            physics->velocity.y = 0;
        }
    }

    const bool played = bt_util::playBranchAnim(behavior, MG_GET_COMPONENT(ctx.entity, AvatarComponent));
    if (!played)
        animationEnd = true;
}

BTStatus HitKindAction::onActionUpdate(BTContext& ctx, int32_t dtMs)
{
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* hr       = MG_GET_COMPONENT(ctx.entity, HitReactComponent);
    auto* physics  = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    if (!behavior)
    {
        return BTStatus::Success;
    }

    if (hr && hr->doubleHitPending)
    {
        hr->doubleHitPending    = false;
        displacementEnd         = false;
        displacementId          = hr->activeDisplacementId;
        const int32_t roleDisp  = roleHitDispId(behavior);
        const int32_t tableType = hr->activeTableHitType;
        const bool airborne     = physics && !physics->onGround;

        if (kind == BehaviorKind::kStun)
        {
            if (roleDisp == -1)
            {
                if (tableType == 0)
                {
                    if (!airborne)
                    {
                        animationEnd = false;
                        replayHitAnim(ctx);
                    }
                    else
                        displacementId = hr->activeDisplacementId;
                }
                else if (animationEnd)
                {
                    return finishToDeathOr(ctx.entity, BehaviorKind::kHitUp);
                }
            }
            else if (roleDisp == 0)
            {
                animationEnd   = false;
                displacementId = -1;
                replayHitAnim(ctx);
            }
            else
            {
                animationEnd = false;
                replayHitAnim(ctx);
            }
            bt_util::startDisplacementId(ctx.entity, displacementId, hr->knockbackFacing, true);
        }
        else if (kind == BehaviorKind::kHitUp)
        {
            bt_util::startDisplacementId(ctx.entity, hr->activeDisplacementId, hr->knockbackFacing, true);
            if (tableType != 1)
                bt_util::dealWithWeight(ctx.entity, hr->activeHitRigidity);
            if (tableType == 1)
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kHitDown);
            }
            if (tableType == 2 || tableType == 0)
                displacementEnd = false;
        }
        else if (kind == BehaviorKind::kHitDown)
        {
            bt_util::startDisplacementId(ctx.entity, hr->activeDisplacementId, hr->knockbackFacing, true);
            if (tableType != 1)
                bt_util::dealWithWeight(ctx.entity, hr->activeHitRigidity);
            if ((tableType == 2 || tableType == 0) && !bt_util::isRigidity(behavior))
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kHitUp);
            }
        }
        else if (kind == BehaviorKind::kHitFloor)
        {
            bt_util::startDisplacementId(ctx.entity, hr->activeDisplacementId, hr->knockbackFacing, true);
            if (tableType != 1)
                bt_util::dealWithWeight(ctx.entity, hr->activeHitRigidity);
            if (tableType == 1)
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kHitDown);
            }
            if ((tableType == 2 || tableType == 0) && !bt_util::isRigidity(behavior))
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kHitUp);
            }
        }
        else if (kind == BehaviorKind::kHitSwitch)
        {
            return finishToDeathOr(ctx.entity, BehaviorKind::kStun);
        }
    }

    if (kind == BehaviorKind::kStun && physics && displacementId <= 0)
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
    }

    pollAnimEnd(ctx.entity, animationEnd);

    if (kind == BehaviorKind::kStun)
    {
        const int32_t tableType = hr ? hr->activeTableHitType : 0;
        const int32_t roleDisp  = roleHitDispId(behavior);
        auto* disp              = MG_GET_COMPONENT(ctx.entity, DisplacementComponent);
        if (disp && disp->airEvent && !animationEnd)
        {
            return finishToDeathOr(ctx.entity, BehaviorKind::kHitDown);
        }
        pollDispBrake(ctx.entity, displacementEnd);
        if ((tableType == 1 || tableType == 2) && animationEnd && roleDisp == -1)
        {
            return finishToDeathOr(ctx.entity, BehaviorKind::kHitUp);
        }

        const bool waitStiff = tableType == 0 || displacementId <= 0 || roleDisp != -1;
        if (waitStiff && animationEnd && (displacementEnd || displacementId <= 0))
        {
            afterEndMs += dtMs;
            int32_t stiff = hr ? hr->activeHitstunMs : 0;
            if (bt_util::isRigidity(behavior))
                stiff = (std::max)(0, stiff - behavior->fatigue * behavior->hitCounts);
            if (afterEndMs >= stiff)
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kHitSwitch);
            }
        }
        return BTStatus::Running;
    }

    if (kind == BehaviorKind::kHitUp)
    {
        pollDispAirOrLie(ctx.entity, displacementEnd);
        if (animationEnd && displacementEnd)
        {
            return finishToDeathOr(ctx.entity, BehaviorKind::kHitDown);
        }
        return BTStatus::Running;
    }

    if (kind == BehaviorKind::kHitDown)
    {
        pollDispLie(ctx.entity, displacementEnd);
        if (animationEnd && displacementEnd)
        {
            return finishToDeathOr(ctx.entity, BehaviorKind::kHitFloor);
        }
        return BTStatus::Running;
    }

    if (kind == BehaviorKind::kHitFloor)
    {
        pollDispBrake(ctx.entity, displacementEnd);
        pollDispLie(ctx.entity, displacementEnd);
        if (animationEnd && displacementEnd)
        {
            if (bt_util::isDeadHp(ctx.entity))
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kGetUp);
            }
            afterEndMs += dtMs;
            int32_t stiff = hr ? hr->activeHitstunMs : 0;
            if (bt_util::isRigidity(behavior))
                stiff = (std::max)(0, stiff - behavior->fatigue * behavior->hitCounts);
            if (afterEndMs >= stiff)
            {
                return finishToDeathOr(ctx.entity, BehaviorKind::kGetUp);
            }
        }
        return BTStatus::Running;
    }

    if (kind == BehaviorKind::kHitSwitch)
    {
        if (animationEnd)
        {
            if (bt_util::isDeadHp(ctx.entity))
            {
                bt_util::enterDeath(ctx.entity);
                return BTStatus::Success;
            }
            bt_util::restoreControlFromHit(ctx.entity);
            bt_util::mixHitKind(ctx.entity, BehaviorKind::kIdle, false);
            return BTStatus::Success;
        }
        return BTStatus::Running;
    }

    if (kind == BehaviorKind::kGetUp)
    {
        if (animationEnd)
        {
            return finishToDeathOr(ctx.entity, BehaviorKind::kWake);
        }
        return BTStatus::Running;
    }

    return BTStatus::Running;
}

void HitKindAction::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(static_cast<int32_t>(kind));
}

bool HitKindAction::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return readKind(byteBuffer, kind);
}

void DeathAction::onActionEnter(BTContext& ctx)
{
    animationEnd    = false;
    displacementEnd = false;
    afterEndMs      = 0;
    bt_util::enterDeath(ctx.entity);
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    displacementId = -1;
    if (behavior && behavior->roleConfig)
        displacementId = behavior->roleConfig->deathDisplacementId;
    bt_util::startDisplacementId(ctx.entity, displacementId, 1.0f);
    const bool played = bt_util::playBranchAnim(behavior, MG_GET_COMPONENT(ctx.entity, AvatarComponent));
    animationEnd      = !played;
}

BTStatus DeathAction::onActionUpdate(BTContext& ctx, int32_t dtMs)
{
    pollAnimEnd(ctx.entity, animationEnd);
    pollDispBrake(ctx.entity, displacementEnd);
    pollDispLie(ctx.entity, displacementEnd);
    if (!(animationEnd && (displacementEnd || displacementId <= 0)))
    {
        return BTStatus::Running;
    }

    afterEndMs += dtMs;
    if (isPlayer(ctx.entity))
    {
        return BTStatus::Running;
    }

    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (behavior)
        behavior->pendingDestroy = true;
    return BTStatus::Success;
}

void TimedKindAction::onActionEnter(BTContext& ctx)
{
    bt_util::setBranchKind(ctx, kind);
    if (auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent))
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
    }
}

BTStatus TimedKindAction::onActionUpdate(BTContext& ctx, int32_t dtMs)
{
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (!behavior)
    {
        return BTStatus::Success;
    }
    if (kind == BehaviorKind::kRevive)
        behavior->reviveRemainMs = (std::max)(0, behavior->reviveRemainMs - dtMs);
    else if (kind == BehaviorKind::kWake)
        behavior->wakeRemainMs = (std::max)(0, behavior->wakeRemainMs - dtMs);
    if (auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent))
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
    }
    bt_util::playBranchAnim(behavior, MG_GET_COMPONENT(ctx.entity, AvatarComponent));
    return BTStatus::Running;
}

void TimedKindAction::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(static_cast<int32_t>(kind));
}

bool TimedKindAction::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return readKind(byteBuffer, kind);
}

void WakeAction::onActionEnter(BTContext& ctx)
{
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent))
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
    }
    if (behavior)
    {
        behavior->rigidityRemain = behavior->rigidityMax;
        if (behavior->roleConfig)
            behavior->rigidityRemain = behavior->roleConfig->rigidity;
        behavior->rigidityMax = behavior->rigidityRemain;
        behavior->hitCounts   = 0;
    }
    if (isPlayer(ctx.entity))
    {
        if (auto* buffMgr = BuffManager::of(ctx.entity))
            buffMgr->addBuff(ctx.entity, kHeroWakeBuffId);
    }
    bt_util::setBranchKind(ctx, BehaviorKind::kIdle);
}

BTStatus WakeAction::onActionUpdate(BTContext& ctx, int32_t /*dtMs*/)
{
    if (bt_util::isDeadHp(ctx.entity))
    {
        bt_util::enterDeath(ctx.entity);
        return BTStatus::Success;
    }
    bt_util::restoreControlFromHit(ctx.entity);
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (behavior)
        behavior->wakeRemainMs = 0;
    return BTStatus::Success;
}

void ReviveAction::onActionEnter(BTContext& ctx)
{
    auto* attr     = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (attr)
    {
        attr->hp = attr->basic.hpMax;
        attr->mp = attr->mpMax;
        attr->ep = attr->epMax;
    }
    if (auto* mgr = SkillManager::of(ctx.entity))
    {
        for (auto& s : mgr->skills)
        {
            if (!s)
                continue;
            s->coolDownMs   = 0;
            s->releaseCount = s->releaseMax > 0 ? s->releaseMax : 1;
        }
        mgr->endCrazy(ctx.entity);
    }
    if (auto* buffMgr = BuffManager::of(ctx.entity))
        buffMgr->addBuff(ctx.entity, kReviveBuffId);
    if (behavior)
    {
        behavior->reviveRequested = false;
        behavior->reviveRemainMs  = 0;
        behavior->pendingDestroy  = false;
        behavior->rigidityRemain  = behavior->roleConfig ? behavior->roleConfig->rigidity : behavior->rigidityMax;
        behavior->rigidityMax     = behavior->rigidityRemain;
        behavior->hitCounts       = 0;
        behavior->statusTags =
            StateTag::kTagGrounded | StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
        behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kIdle);
        behavior->currentBranchIndex = -1;
    }
    bt_util::playBranchAnim(behavior, MG_GET_COMPONENT(ctx.entity, AvatarComponent));
}

BTStatus ReviveAction::onActionUpdate(BTContext& /*ctx*/, int32_t /*dtMs*/)
{
    return BTStatus::Success;
}

void HoldAttackAction::onActionEnter(BTContext& ctx)
{
    if (auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent))
    {
        behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kAttack);
        behavior->currentBranchIndex = -1;
    }
}

BTStatus HoldAttackAction::onActionUpdate(BTContext& /*ctx*/, int32_t /*dtMs*/)
{
    return BTStatus::Running;
}

NS_MG_END
