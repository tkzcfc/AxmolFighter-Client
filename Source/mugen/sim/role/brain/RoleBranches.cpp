#include "RoleBranches.h"
#include "RoleBrainRunner.h"

#include "mugen/sim/World.h"
#include "mugen/sim/skill/SkillCaster.h"

namespace mugen::sim::role::brain
{
namespace
{
using S = EntityRoleStatus;
// 各分支对应的状态守卫，顺序与 RoleBranch 枚举一致。
constexpr std::array<S, kBranchCount> kBranchStatuses = {
    S::Death,     S::Revive, S::Wake,    S::GetUp,   S::HitFloor, S::HitDown, S::HitUp,
    S::HitSwitch, S::Hit,    S::Attack,  S::Jostled, S::Patrol,   S::Chase,   S::Alert,
    S::Finding,   S::Run,    S::Walk,    S::Idle,    S::Controlled};
// 各分支对应的叶子动作类型。
constexpr std::array<behavior::ActionType, kBranchCount> kBranchActions = {
    behavior::ActionType::Death,   behavior::ActionType::Revive,    behavior::ActionType::Wake,
    behavior::ActionType::GetUp,   behavior::ActionType::HitFloor,  behavior::ActionType::HitDown,
    behavior::ActionType::HitUp,   behavior::ActionType::HitSwitch, behavior::ActionType::Hit,
    behavior::ActionType::None,    behavior::ActionType::Jostled,   behavior::ActionType::Patrol,
    behavior::ActionType::Chase,   behavior::ActionType::Alert,     behavior::ActionType::Move,
    behavior::ActionType::Run,     behavior::ActionType::Walk,      behavior::ActionType::Idle,
    behavior::ActionType::Controlled};
}  // namespace

behavior::ActionType RoleBrainDriver::_branchAction(RoleBranch branch) const
{
    return kBranchActions[static_cast<size_t>(branch)];
}

bool RoleBrainDriver::_guardHolds(RoleBranch branch) const
{
    const auto status = kBranchStatuses[static_cast<size_t>(branch)];
    const auto& r     = m_world.ecs().get<RoleStatus>(m_entity);
    if (!r.has(status))
        return false;
    switch (status)
    {
    case S::Alert:
        return !r.isTobeHit();
    case S::Run:
    case S::Walk:
        return !r.isAttack() && !r.isJostled();
    case S::Idle:
        if (m_city)
            return !r.isMove();
        return !r.isAttack() && !r.isWake() && !r.isTobeHit() &&
               !r.isMove() && !r.isJostled();
    default:
        return true;
    }
}

void RoleBrainDriver::_exitGuard(RoleBranch branch)
{
    const auto status = kBranchStatuses[static_cast<size_t>(branch)];
    if (status == S::Attack)
        skill::SkillCaster(m_world, m_entity).resetSkill();
    if (status != S::Idle && status != S::Revive)
        m_world.ecs().get<RoleStatus>(m_entity).abort(status);
}

behavior::ActionType RoleBrainDriver::currentAction() const
{
    return _branchAction(m_brain.m_branch);
}
}  // namespace mugen::sim::role::brain
