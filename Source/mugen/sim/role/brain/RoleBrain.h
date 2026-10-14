#pragma once

#include "mugen/core/bt/BTInstance.h"
#include "mugen/sim/ai/ActionMove.h"
#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/input/PlayerCommand.h"

namespace mugen::sim::role::brain
{
// 角色脑分支，顺序即根选择器的循环选择顺序（RoleTree.cpp build 的 statuses 列表）。
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
// 叶子状态数据；攻击叶状态放在桥接 B 的 BT 实例内，故不在此出现。
using RoleStateData = std::variant<std::monostate, behavior::ActionState, behavior::JostledState,
                                   behavior::HitActionState, ai::ActionMove>;
// 受击叶子退出后遗留的位移监听，事件仍路由到已退出叶子的数据。
struct DetachedHitListener
{
    behavior::ActionType m_type = behavior::ActionType::None;
    behavior::HitActionState m_state;
    bool m_active = false;
    MG_REFLECT(DetachedHitListener, m_type, m_state, m_active)
};
// 临时桥接 B：攻击子树专用 BT 实例，阶段 3 删除。
struct LegacyAttackTree
{
    behavior::DefinitionKey m_key;
    BTInstance<behavior::LeafState> m_instance;
    MG_REFLECT(LegacyAttackTree, m_key, m_instance)
};
// 角色脑组件：枚举状态机驱动的战斗脑。
struct RoleBrain
{
    BrainPhase m_phase = BrainPhase::NotEntered;
    RoleBranch m_branch = RoleBranch::Idle;
    // Death 序列的第二段（销毁叶）。
    int32_t m_destroyStage = 0;
    RoleStateData m_data;
    // 出生技能装配快照，用于重建攻击子树。
    behavior::DefinitionKey m_attackLayout;
    LegacyAttackTree m_cast;
    DetachedHitListener m_detached;
    MG_REFLECT(RoleBrain, m_phase, m_branch, m_destroyStage, m_data, m_attackLayout, m_cast, m_detached)
};
// 角色控制：保存最近一条玩家命令。
struct RoleControl
{
    PlayerCommand m_last;
    MG_REFLECT(RoleControl, m_last)
};
}  // namespace mugen::sim::role::brain
