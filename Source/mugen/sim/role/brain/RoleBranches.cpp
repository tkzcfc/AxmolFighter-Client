#include "RoleBranches.h"

#include "mugen/sim/World.h"
#include "mugen/sim/skill/Skill.h"

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

behavior::ActionType branchAction(RoleBranch branch)
{
    return kBranchActions[static_cast<size_t>(branch)];
}

bool guardHolds(RoleBranch branch, World& world, Entity entity, bool city)
{
    const auto status = kBranchStatuses[static_cast<size_t>(branch)];
    const auto& r     = world.ecs().get<RoleStatus>(entity);
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
        if (city)
            return !r.isMove();
        return !r.isAttack() && !r.isWake() && !r.isTobeHit() &&
               !r.isMove() && !r.isJostled();
    default:
        return true;
    }
}

void exitGuard(RoleBranch branch, World& world, Entity entity)
{
    const auto status = kBranchStatuses[static_cast<size_t>(branch)];
    if (status == S::Attack)
        skill::resetSkill(world, entity);
    if (status != S::Idle && status != S::Revive)
        world.ecs().get<RoleStatus>(entity).abort(status);
}
}  // namespace mugen::sim::role::brain
