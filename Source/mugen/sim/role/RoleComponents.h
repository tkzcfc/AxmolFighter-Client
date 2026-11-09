#pragma once
#include "mugen/sim/role/RoleState.h"
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec3.h"

namespace mugen::sim
{
// 角色状态机组件：唯一转移者 RoleFsmSystem 写状态，Death/Room 只写请求位。
struct RoleState
{
    // 当前状态。
    RoleStateId m_state = RoleStateId::Idle;
    // 上一帧状态。
    RoleStateId m_prev = RoleStateId::Idle;
    // 进入当前状态后累计的毫秒。
    Fixed m_stateMs;
    // 死亡请求位（DeathSystem 写，FSM 读）。
    bool m_deathRequested = false;
    // 复活请求位（Room 写，FSM 读）。
    bool m_reviveRequested = false;
    MG_REFLECT(RoleState, m_state, m_prev, m_stateMs, m_deathRequested, m_reviveRequested)
};
// 受击与击退流程的运行时状态（B2 实现 Hit/HitSwitch 状态机）。
struct HitState
{
    // 当前韧性值。
    Fixed m_rigidity;
    // 当前重量值。
    Fixed m_weight;
    // 当前疲劳值。
    Fixed m_fatigue;
    // 已累计的受击次数。
    int32_t m_hitCounts = 0;
    // 硬直持续时间（读取时逐次扣除疲劳）。
    Fixed m_stiffTime;
    // 待处理受击请求（CombatSystem 写，RoleFsm 消费）。
    int32_t m_pendingHitDefId = 0;         // 0 表示无
    int32_t m_pendingHitType = -1;
    int32_t m_pendingDisplacementId = -1;
    int32_t m_pendingTowardX = 1;
    // 受击状态机运行数据。
    int32_t m_hitDefId = 0;
    int32_t m_displacementId = -1;
    bool m_animationEnd = false;
    bool m_displacementEnd = false;
    Fixed m_hitTimeMs;
    // 受击来源朝向。
    int32_t m_towardX = 1;
    MG_REFLECT(HitState, m_rigidity, m_weight, m_fatigue, m_hitCounts, m_stiffTime, m_pendingHitDefId,
               m_pendingHitType, m_pendingDisplacementId, m_pendingTowardX, m_hitDefId, m_displacementId,
               m_animationEnd, m_displacementEnd, m_hitTimeMs, m_towardX)
};
}  // namespace mugen::sim
