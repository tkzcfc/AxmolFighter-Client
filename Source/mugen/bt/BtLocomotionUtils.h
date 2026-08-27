#pragma once

#include "mugen/core/bt/BTContext.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class BehaviorComponent;
class AvatarComponent;
class InputComponent;
class Entity;

// 移动输入、分支动画、走/跑速度。供角色树叶子与 System 共用。
namespace bt_util
{

// 双击同向判定窗口
constexpr int32_t kMoveDoubleTapMs = 400;
// 跑相对走的速度倍率
constexpr float kRunRate = 1.6f;
// 空中水平控制倍率
constexpr float kAirControl = 0.8f;
// 落地后锁移动输入
constexpr int32_t kLandLockMs = 80;

// 本帧刚按下
bool justPressed(const InputComponent* input, int32_t slot);

// 本帧抬起（上一帧按下且当前未按下）
bool justReleased(const InputComponent* input, int32_t slot);

// 按 slotTriggerFlags 判定本帧是否触发（flags==0 视为 Press）
bool slotTriggered(const InputComponent* input, int32_t slot, uint32_t slotTriggerFlags);

// 移动键象限：1=右 2=上 3=左 4=下；0=无
int32_t moveQuadrantFromInput(const InputComponent* input);
bool anyMoveKeyDown(const InputComponent* input);
bool anyMoveJustPressed(const InputComponent* input);
bool isSameSide(int32_t a, int32_t b);

// 按 currentKind 选 Behavior 分支动画
bool playBranchAnim(BehaviorComponent* behavior, AvatarComponent* avatar);

// 清 currentBranchIndex 后重播
void invalidateBranchAndPlay(BehaviorComponent* behavior, AvatarComponent* avatar);

// 按输入写物理水平速度（含跑/空中倍率）
void applyLocomotionVelocity(BTContext& ctx);

// 切换 BehaviorComponent.currentKind 并播对应动画
void setBranchKind(BTContext& ctx, BehaviorKind kind);

bool isDeadHp(Entity* entity);
bool isRigidity(const BehaviorComponent* behavior);
void dealWithWeight(Entity* entity, int32_t hitRigidity);
void enterDeath(Entity* entity);
void mixHitKind(Entity* entity, BehaviorKind kind, bool playAnim = true);
void restoreControlFromHit(Entity* entity);
void startDisplacementId(Entity* entity, int32_t displacementId, float facingSign, bool applyZRate = false);

}  // namespace bt_util

NS_MG_END
