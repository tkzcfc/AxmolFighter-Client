#pragma once
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleState.h"

namespace mugen::sim
{
namespace role::rules
{
// 唯一状态写入：更新状态、清零计时并发出带正确 m_from 的 StateEvent（含 BehaviorState 触发）。
void transition(World& world, Entity entity, RoleStateComponent& state, RoleStateId to);
// 播放待机动作。
void playIdle(MotionStateComponent& motion);
// 施法结束回到待机或跑动：nextRun 且跑动意图时转 Run，否则回 Idle。
void exitAttack(World& world, Entity entity, RoleStateComponent& state, SkillCastComponent& cast,
                MotionStateComponent& motion, const IntentComponent& intent);
}  // namespace role::rules
}  // namespace mugen::sim
