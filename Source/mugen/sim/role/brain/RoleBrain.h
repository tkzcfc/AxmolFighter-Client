#pragma once

#include "mugen/sim/ai/ActionMove.h"
#include "mugen/sim/attack/AttackState.h"
#include "mugen/sim/bt/HitActionState.h"
#include "mugen/sim/input/PlayerCommand.h"
#include "mugen/sim/role/brain/RoleAction.h"
#include <variant>
#include <vector>

namespace mugen::sim::role::brain
{
// 角色脑分支，顺序即循环选择顺序。
enum class RoleBranch : uint8_t
{
    Death,
    Revive,
    Wake,
    GetUp,
    HitFloor,
    HitDown,
    HitUp,
    HitSwitch,
    Hit,
    Attack,
    Jostled,
    Patrol,
    Chase,
    Alert,
    Finding,
    Run,
    Walk,
    Idle,
    Controlled
};
// 脑运行阶段。
enum class BrainPhase : uint8_t
{
    NotEntered,
    Running,
    Stopped
};
// 状态单帧推进结果。
enum class StateStep : uint8_t
{
    Running,
    Done
};
// 叶子状态数据；攻击叶放在施法计划里。
using RoleStateData = std::
    variant<std::monostate, behavior::ActionState, behavior::JostledState, behavior::HitActionState, ai::ActionMove>;
// 一次施法的动作序列与当前攻击叶。
struct CastPlan
{
    // 本次管线对应的技能下标；未施法时为 -1。
    int32_t m_skill = -1;
    // 进入管线时匹配到的管线编号。
    int32_t m_pipe = 1;
    // 是否走松开动作列。
    bool m_release = false;
    // 当前方向列的动作配置。
    std::vector<int32_t> m_actions;
    // 当前动作在 m_actions 中的下标。
    int32_t m_index = 0;
    // 当前攻击叶的运行状态。
    attack::AttackRole m_leaf;
    MG_REFLECT(CastPlan, m_skill, m_pipe, m_release, m_actions, m_index, m_leaf)
};
// 角色脑组件：枚举状态机驱动的战斗脑。
struct RoleBrain
{
    BrainPhase m_phase  = BrainPhase::NotEntered;
    RoleBranch m_branch = RoleBranch::Idle;
    // Death 序列的第二段（销毁叶）。
    int32_t m_destroyStage = 0;
    RoleStateData m_data;
    // 攻击分支的施法计划。
    CastPlan m_cast;
    // 本轮无可进入分支时是否已记录错误日志。
    bool m_noBranchLogged = false;
    MG_REFLECT(RoleBrain, m_phase, m_branch, m_destroyStage, m_data, m_cast, m_noBranchLogged)
};
// 角色控制：保存最近一条玩家命令。
struct RoleControl
{
    PlayerCommand m_last;
    MG_REFLECT(RoleControl, m_last)
};
}  // namespace mugen::sim::role::brain
