#include "mugen/bt/BtLocomotionUtils.h"

#include "mugen/Components.h"

#include <cmath>

NS_MG_BEGIN

namespace bt_util
{

bool justPressed(const InputComponent* input, int32_t slot)
{
    return input && input->isKeyDown(slot) && input->queryKeyPressedDurationMs(slot) == 0;
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

bool playBranchAnim(BehaviorComponent* behavior, AvatarComponent* avatar)
{
    if (!behavior || !behavior->behaviorTemplate || !avatar)
        return false;
    const auto& branches = behavior->behaviorTemplate->branches;
    for (size_t i = 0; i < branches.size(); ++i)
    {
        const auto& b = branches[i];
        if (b.kind != behavior->currentKind)
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

void invalidateBranchAndPlay(BehaviorComponent* behavior, AvatarComponent* avatar)
{
    if (!behavior)
        return;
    behavior->currentBranchIndex = -1;
    playBranchAnim(behavior, avatar);
}

void setBranchKind(BTContext& ctx, BehaviorKind kind)
{
    if (!ctx.behavior)
        return;
    const int32_t k = static_cast<int32_t>(kind);
    if (ctx.behavior->currentKind != k)
    {
        ctx.behavior->currentKind        = k;
        ctx.behavior->currentBranchIndex = -1;
    }
    if (ctx.bt)
        ctx.bt->activeBranchKind = k;
    playBranchAnim(ctx.behavior, ctx.avatar);
}

void applyLocomotionVelocity(BTContext& ctx)
{
    auto* behavior  = ctx.behavior;
    auto* physics   = ctx.physics;
    auto* input     = ctx.input;
    auto* attribute = ctx.attribute;
    auto* transform = ctx.transform;
    if (!behavior || !physics || !input)
        return;

    if ((behavior->statusTags & StateTag::kTagMovable) == 0 || behavior->landLockMs > 0)
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
        return;
    }

    float speed = 300.0f;
    if (attribute)
        speed = attribute->currentAttribute.moveSpeed;
    else if (behavior->roleConfig)
        speed = behavior->roleConfig->attribute.moveSpeed;

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

}  // namespace bt_util

NS_MG_END
