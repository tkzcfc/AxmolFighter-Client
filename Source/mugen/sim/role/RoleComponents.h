#pragma once
#include "mugen/sim/role/RoleState.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/FixedVec3.h"
#include <optional>

namespace mugen::sim
{
// 角色状态机组件：唯一转移者 RoleFsmSystem 写状态，Death/Room 只写请求位。
struct RoleStateComponent
{
    // 当前状态。
    RoleStateId state = RoleStateId::Idle;
    // 上一帧状态。
    RoleStateId prev = RoleStateId::Idle;
    // 进入当前状态后累计的毫秒。
    Fixed stateMs;
    // 死亡请求位（DeathSystem 写，FSM 读）。
    bool deathRequested = false;
    // 复活请求位（Room 写，FSM 读）。
    bool reviveRequested = false;
    // 死亡事实是否已宣布（DeathSystem 写）。
    bool deathAnnounced = false;
    // 击杀者（DeathSystem 从 DeathRequest 写入）。
    Entity deathKiller;
    // 木偶同步请求的目标状态（InputSystem 写，FSM 消费）。
    RoleStateId puppetState = RoleStateId::Idle;
    // 本帧有木偶同步请求。
    bool puppetRequested = false;
    // 连续移动秒数（只计跑）。
    Fixed movedSec;
    // 连续站立秒数。
    Fixed standingSec;
    MG_REFLECT(RoleStateComponent, state, prev, stateMs, deathRequested, reviveRequested, deathAnnounced,
               deathKiller, puppetState, puppetRequested, movedSec, standingSec)
};
// 受击与击退流程的运行时状态（B2 实现 Hit/HitSwitch 状态机）。
struct HitStateComponent
{
    // 当前韧性值。
    Fixed rigidity;
    // 当前重量值。
    Fixed weight;
    // 当前疲劳值。
    Fixed fatigue;
    // 已累计的受击次数。
    int32_t hitCounts = 0;
    // 硬直持续时间（读取时逐次扣除疲劳）。
    Fixed stiffTime;
    // 待处理受击请求（CombatSystem 写，RoleFsm 消费）。
    int32_t pendingHitDefId = 0;         // 0 表示无
    int32_t pendingHitType = -1;
    int32_t pendingDisplacementId = -1;
    int32_t pendingTowardX = 1;
    // 受击状态机运行数据。
    int32_t hitDefId = 0;
    // 当前受击的击飞类型（0 击退 / 1 击倒 / 2 击飞 / -1 无受击）。
    int32_t hitType = -1;
    int32_t displacementId = -1;
    bool animationEnd = false;
    bool displacementEnd = false;
    Fixed hitTimeMs;
    // 受击来源朝向。
    int32_t towardX = 1;
    // 受击/死亡状态已转移但 enter 动作（动作/位移）被顿帧推迟，待解冻后补做。
    bool enterPending = false;
    // 是否已执行受击保护。
    bool hitProtectExecuted = false;
    // 受击保护的生命阈值。
    std::optional<Fixed> hitProtectHp;
    // 受击保护清理倒计时。
    Fixed hitProtectTime;
    MG_REFLECT(HitStateComponent, rigidity, weight, fatigue, hitCounts, stiffTime, pendingHitDefId,
               pendingHitType, pendingDisplacementId, pendingTowardX, hitDefId, hitType, displacementId,
               animationEnd, displacementEnd, hitTimeMs, towardX, enterPending, hitProtectExecuted, hitProtectHp,
               hitProtectTime)
};
}  // namespace mugen::sim
