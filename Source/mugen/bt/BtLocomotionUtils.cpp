#include "mugen/bt/BtLocomotionUtils.h"

#include "mugen/Components.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace bt_util
{

bool justPressed(const InputComponent* input, int32_t slot)
{
    return input && input->isKeyDown(slot) && input->queryKeyPressedDurationMs(slot) == 0;
}

bool justReleased(const InputComponent* input, int32_t slot)
{
    return input && input->isLastKeyDown(slot) && !input->isKeyDown(slot);
}

bool slotTriggered(const InputComponent* input, int32_t slot, uint32_t slotTriggerFlags)
{
    if (!input || slot <= 0)
        return false;
    uint32_t flags = slotTriggerFlags;
    if (flags == SlotTriggerFlag::kSlotTriggerNone)
        flags = SlotTriggerFlag::kSlotTriggerPress;

    if ((flags & SlotTriggerFlag::kSlotTriggerPress) && justPressed(input, slot))
        return true;
    if ((flags & SlotTriggerFlag::kSlotTriggerKeepPress) && input->isKeyDown(slot) && !justPressed(input, slot))
        return true;
    if ((flags & SlotTriggerFlag::kSlotTriggerRelease) && justReleased(input, slot))
        return true;
    return false;
}

int32_t moveQuadrantFromInput(const InputComponent* input)
{
    if (!input)
        return 0;
    const bool left  = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT));
    const bool right = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT));
    const bool up    = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP));
    const bool down  = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN));

    float vx = 0.0f, vy = 0.0f;
    if (left && right)
        vx = (input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) <=
              input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
                 ? -1.0f
                 : 1.0f;
    else if (left)
        vx = -1.0f;
    else if (right)
        vx = 1.0f;

    if (up && down)
        vy = (input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) <=
              input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
                 ? 1.0f
                 : -1.0f;
    else if (up)
        vy = 1.0f;
    else if (down)
        vy = -1.0f;

    if (vx == 0.0f && vy == 0.0f)
        return 0;
    if (std::fabs(vx) >= std::fabs(vy))
        return vx > 0.0f ? 1 : 3;
    return vy > 0.0f ? 2 : 4;
}

bool anyMoveKeyDown(const InputComponent* input)
{
    if (!input)
        return false;
    return input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) ||
           input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)) ||
           input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) ||
           input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN));
}

bool anyMoveJustPressed(const InputComponent* input)
{
    return justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) ||
           justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)) ||
           justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) ||
           justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN));
}

bool isSameSide(int32_t a, int32_t b)
{
    return a != 0 && a == b;
}

int32_t locomotionAnimKind(int32_t kind)
{
    switch (static_cast<BehaviorKind>(kind))
    {
    case BehaviorKind::kPatrol:
    case BehaviorKind::kChase:
    case BehaviorKind::kAlert:
    case BehaviorKind::kPathFinding:
    case BehaviorKind::kJostled:
        return static_cast<int32_t>(BehaviorKind::kWalk);
    case BehaviorKind::kRevive:
    case BehaviorKind::kWake:
        return static_cast<int32_t>(BehaviorKind::kIdle);
    default:
        return kind;
    }
}

bool playBranchAnimForKind(BehaviorComponent* behavior, AvatarComponent* avatar, int32_t kind)
{
    if (!behavior || !behavior->behaviorTemplate || !avatar)
        return false;
    const auto& branches = behavior->behaviorTemplate->branches;
    for (size_t i = 0; i < branches.size(); ++i)
    {
        const auto& b = branches[i];
        if (b.kind != kind)
            continue;
        if (b.requireTags && (behavior->statusTags & b.requireTags) != b.requireTags)
            continue;
        if (b.denyTags && (behavior->statusTags & b.denyTags) != 0)
            continue;
        if (behavior->currentBranchIndex != static_cast<int32_t>(i) && !b.animation.empty())
            avatar->play(b.animation, b.loop ? -1 : 1, false);
        behavior->currentBranchIndex = static_cast<int32_t>(i);
        return true;
    }
    return false;
}

bool playBranchAnim(BehaviorComponent* behavior, AvatarComponent* avatar)
{
    if (!behavior)
        return false;
    if (playBranchAnimForKind(behavior, avatar, behavior->currentKind))
        return true;
    const int32_t fallback = locomotionAnimKind(behavior->currentKind);
    if (fallback != behavior->currentKind)
        return playBranchAnimForKind(behavior, avatar, fallback);
    return false;
}

void invalidateBranchAndPlay(BehaviorComponent* behavior, AvatarComponent* avatar)
{
    if (!behavior)
        return;
    behavior->currentBranchIndex = -1;
    playBranchAnim(behavior, avatar);
}

void setBranchKind(BTContext& ctx, BehaviorKind kind)
{
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (!behavior)
        return;
    const int32_t k = static_cast<int32_t>(kind);
    if (behavior->currentKind != k)
    {
        behavior->currentKind        = k;
        behavior->currentBranchIndex = -1;
    }
    auto* avatar = MG_GET_COMPONENT(ctx.entity, AvatarComponent);
    playBranchAnim(behavior, avatar);
}

void applyLocomotionVelocity(BTContext& ctx)
{
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* physics   = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    auto* input     = MG_GET_COMPONENT(ctx.entity, InputComponent);
    auto* attribute = MG_GET_COMPONENT(ctx.entity, AttributeComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (!behavior || !physics || !input)
        return;

    float speed = 300.0f;
    if (attribute)
        speed = attribute->moveSpeed;

    if (behavior->statusTags & StateTag::kTagDashState)
        speed *= kRunRate;
    if (behavior->statusTags & StateTag::kTagAirborne)
        speed *= kAirControl;

    const int32_t leftSlot  = static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT);
    const int32_t rightSlot = static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT);
    const int32_t upSlot    = static_cast<int32_t>(INPUT_SLOT_MOVE_UP);
    const int32_t downSlot  = static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN);

    const bool left  = input->isKeyDown(leftSlot);
    const bool right = input->isKeyDown(rightSlot);
    const bool up    = input->isKeyDown(upSlot);
    const bool down  = input->isKeyDown(downSlot);

    float vx = 0.0f;
    if (left && right)
        vx = (input->queryKeyPressedDurationMs(leftSlot) <= input->queryKeyPressedDurationMs(rightSlot)) ? -1.0f : 1.0f;
    else if (left)
        vx = -1.0f;
    else if (right)
        vx = 1.0f;

    float vy = 0.0f;
    if (up && down)
        vy = (input->queryKeyPressedDurationMs(upSlot) <= input->queryKeyPressedDurationMs(downSlot)) ? 1.0f : -1.0f;
    else if (up)
        vy = 1.0f;
    else if (down)
        vy = -1.0f;

    if (vx != 0 || vy != 0)
    {
        const float len = std::sqrt(vx * vx + vy * vy);
        vx              = vx / len * speed;
        vy              = vy / len * speed;
        if (transform && vx != 0 && (behavior->statusTags & StateTag::kTagFacingAllowed))
            transform->facingDirection = vx > 0 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
    }
    physics->velocity.x = vx;
    physics->velocity.y = vy;
}

bool isDeadHp(Entity* entity)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    return attr && attr->hp <= 0.0f;
}

bool isRigidity(const BehaviorComponent* behavior)
{
    return behavior && behavior->rigidityRemain <= 0 && behavior->rigidityMax > 0;
}

void dealWithWeight(Entity* entity, int32_t hitRigidity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!behavior)
        return;
    behavior->rigidityRemain = (std::max)(0, behavior->rigidityRemain - hitRigidity);
    if (isDeadHp(entity) || !isRigidity(behavior))
        return;
    const float dump = behavior->weight * static_cast<float>(behavior->hitCounts);
    if (auto* disp = MG_GET_COMPONENT(entity, DisplacementComponent))
        disp->velocity.y -= dump;
    if (auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent))
        physics->velocity.z -= dump * DisplacementComponent::kLuaVelToPhysics;
}

void enterDeath(Entity* entity)
{
    if (auto* mgr = SkillManager::of(entity))
        mgr->forceInterruptCast(entity);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* hitReact = MG_GET_COMPONENT(entity, HitReactComponent);
    if (hitReact)
        hitReact->pendingHits.clear();
    if (behavior)
    {
        behavior->statusTags &= ~(StateTag::kTagHitState | StateTag::kTagDownState | StateTag::kTagAttackState |
                                  StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagDashState);
        behavior->hitStunRemainingMs = 0;
        behavior->downRemainMs       = 0;
        behavior->getUpRemainMs      = 0;
        behavior->hitSwitchRemainMs  = 0;
        behavior->wakeRemainMs       = 0;
        if (behavior->currentKind != static_cast<int32_t>(BehaviorKind::kDeath))
        {
            if (auto* buffMgr = BuffManager::of(entity))
                buffMgr->trigger(entity, BFEvent::BeforeDeath, nullptr, 0);
            const int32_t oldKind        = behavior->currentKind;
            behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kDeath);
            behavior->currentBranchIndex = -1;
            BuffRuleUtil::notifyBehaviorKindChange(entity, oldKind, behavior->currentKind);
        }
    }
    if (auto* hr = MG_GET_COMPONENT(entity, HitReactComponent))
    {
        hr->activeHitType        = HitType::kHitNone;
        hr->activeTableHitType   = 0;
        hr->activeHitstunMs      = 0;
        hr->doubleHitPending     = false;
        hr->activeDisplacementId = -1;
    }
}

void mixHitKind(Entity* entity, BehaviorKind kind, bool playAnim)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* physics  = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    if (!behavior)
        return;

    const int32_t oldKind        = behavior->currentKind;
    behavior->currentKind        = static_cast<int32_t>(kind);
    behavior->currentBranchIndex = -1;
    behavior->downRemainMs       = 0;
    behavior->getUpRemainMs      = 0;
    behavior->hitSwitchRemainMs  = 0;

    if (kind == BehaviorKind::kHitUp || kind == BehaviorKind::kHitDown)
    {
        behavior->statusTags |= StateTag::kTagAirborne | StateTag::kTagHitState;
        behavior->statusTags &= ~(StateTag::kTagGrounded | StateTag::kTagDownState);
        if (physics && kind == BehaviorKind::kHitUp)
            physics->onGround = 0;
    }
    else if (kind == BehaviorKind::kHitFloor)
    {
        behavior->statusTags |= StateTag::kTagDownState | StateTag::kTagHitState | StateTag::kTagGrounded;
        behavior->statusTags &= ~(StateTag::kTagAirborne | StateTag::kTagFalling);
    }
    else if (kind == BehaviorKind::kGetUp)
    {
        behavior->statusTags |= StateTag::kTagDownState | StateTag::kTagHitState | StateTag::kTagGrounded;
        behavior->statusTags &= ~(StateTag::kTagAirborne | StateTag::kTagFalling);
        behavior->getUpRemainMs = 1;
    }
    else if (kind == BehaviorKind::kWake)
    {
        behavior->statusTags &= ~(StateTag::kTagHitState | StateTag::kTagDownState | StateTag::kTagAirborne);
        behavior->statusTags |= StateTag::kTagGrounded;
        behavior->wakeRemainMs = 1;
    }
    else if (kind == BehaviorKind::kHitSwitch || kind == BehaviorKind::kStun)
    {
        behavior->statusTags |= StateTag::kTagHitState;
        behavior->statusTags &= ~StateTag::kTagDownState;
    }

    BuffRuleUtil::notifyBehaviorKindChange(entity, oldKind, behavior->currentKind);
    if (playAnim)
        playBranchAnim(behavior, avatar);
}

void restoreControlFromHit(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!behavior)
        return;
    behavior->statusTags &= ~(StateTag::kTagHitState | StateTag::kTagDownState);
    behavior->statusTags |=
        StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed | StateTag::kTagGrounded;
    if (auto* hr = MG_GET_COMPONENT(entity, HitReactComponent))
    {
        hr->activeHitType        = HitType::kHitNone;
        hr->activeTableHitType   = 0;
        hr->doubleHitPending     = false;
        hr->activeDisplacementId = -1;
    }
    behavior->hitStunRemainingMs = 0;
    behavior->wakeRemainMs       = 0;
    behavior->getUpRemainMs      = 0;
    behavior->hitCounts          = 0;
}

void startDisplacementId(Entity* entity, int32_t displacementId, float facingSign, bool applyZRate)
{
    auto* disp = MG_GET_COMPONENT(entity, DisplacementComponent);
    if (!disp || displacementId <= 0)
        return;
    const auto* cfg = Config::getInstance()->getDisplacementConfigById(displacementId);
    if (!cfg)
        return;
    auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent);
    disp->restoreGravity(physics);
    disp->start(cfg, facingSign);
    if (applyZRate)
        disp->velocity.z *= kZRate;
    disp->writePhysicsVelocity(physics, facingSign);
}

}  // namespace bt_util

NS_MG_END
