#pragma once

#include "mugen/core/bt/BTContext.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class BehaviorComponent;
class AvatarComponent;
class InputComponent;

namespace bt_util
{

constexpr int32_t kMoveDoubleTapMs = 400;
constexpr float kRunRate           = 1.6f;
constexpr float kAirControl        = 0.8f;
constexpr int32_t kLandLockMs      = 80;
constexpr int32_t kDownMs          = 400;
constexpr int32_t kGetUpMs         = 350;

bool justPressed(const InputComponent* input, int32_t slot);
int32_t moveQuadrantFromInput(const InputComponent* input);
bool anyMoveKeyDown(const InputComponent* input);
bool anyMoveJustPressed(const InputComponent* input);
bool isSameSide(int32_t a, int32_t b);

bool playBranchAnim(BehaviorComponent* behavior, AvatarComponent* avatar);
void invalidateBranchAndPlay(BehaviorComponent* behavior, AvatarComponent* avatar);

void applyLocomotionVelocity(BTContext& ctx);
void setBranchKind(BTContext& ctx, BehaviorKind kind);

}  // namespace bt_util

NS_MG_END
