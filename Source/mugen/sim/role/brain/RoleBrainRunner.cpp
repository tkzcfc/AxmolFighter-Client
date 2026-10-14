#include "RoleBrainRunner.h"

#include "RoleBranches.h"
#include "mugen/sim/World.h"
#include "mugen/sim/motion/Displacement.h"
#include "mugen/sim/role/brain/RoleContext.h"
#include "mugen/sim/role/brain/states/AiMoveState.h"
#include "mugen/sim/role/brain/states/CastState.h"
#include "mugen/sim/role/brain/states/HitStates.h"
#include "mugen/sim/role/brain/states/LocomotionStates.h"

namespace mugen::sim::role::brain
{
namespace
{
// 按分支重置叶子状态数据为对应类型。
void resetData(RoleBrain& brain)
{
    brain.m_destroyStage = 0;
    switch (brain.m_branch)
    {
    case RoleBranch::Death:
    case RoleBranch::Revive:
    case RoleBranch::Wake:
    case RoleBranch::GetUp:
    case RoleBranch::HitFloor:
    case RoleBranch::HitDown:
    case RoleBranch::HitUp:
    case RoleBranch::HitSwitch:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
        brain.m_data = behavior::HitActionState{};
        break;
    case RoleBranch::Jostled:
        brain.m_data = behavior::JostledState{};
        break;
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        brain.m_data = ai::ActionMove{};
        break;
    case RoleBranch::Finding:
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        brain.m_data = behavior::ActionState{};
        break;
    case RoleBranch::Attack:
        break;
    }
}
// 进入当前分支的叶子；返回是否成功（仅 Attack 可能失败）。
bool enterState(RoleBrain& brain, World& world, Entity entity)
{
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(brain.m_branch));
    const auto action = branchAction(brain.m_branch);
    switch (brain.m_branch)
    {
    case RoleBranch::Death:
    case RoleBranch::Revive:
    case RoleBranch::Wake:
    case RoleBranch::GetUp:
    case RoleBranch::HitFloor:
    case RoleBranch::HitDown:
    case RoleBranch::HitUp:
    case RoleBranch::HitSwitch:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
        states::HitState::enter(rc, action, std::get<behavior::HitActionState>(brain.m_data));
        return true;
    case RoleBranch::Jostled:
        states::Jostled::enter(rc, action, std::get<behavior::JostledState>(brain.m_data));
        return true;
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        states::AiMove::enter(rc, action, std::get<ai::ActionMove>(brain.m_data));
        return true;
    case RoleBranch::Finding:
        states::Finding::enter(rc, action, std::get<behavior::ActionState>(brain.m_data));
        return true;
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        states::Locomotion::enter(rc, action, std::get<behavior::ActionState>(brain.m_data));
        return true;
    case RoleBranch::Attack:
        return states::CastState::enter(brain, world, entity);
    }
    return false;
}
// 推进当前分支的叶子；Death 分支按两段（Death 后 Destroy）推进。
StateStep updateState(RoleBrain& brain, World& world, Entity entity)
{
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(brain.m_branch));
    const auto action = branchAction(brain.m_branch);
    const auto step   = [](BTStatus status) { return status == BTStatus::Running ? StateStep::Running : StateStep::Done; };
    switch (brain.m_branch)
    {
    case RoleBranch::Death:
        if (brain.m_destroyStage == 0)
        {
            if (states::HitState::update(rc, action, std::get<behavior::HitActionState>(brain.m_data)) == BTStatus::Running)
                return StateStep::Running;
            brain.m_destroyStage = 1;
            states::HitState::enter(rc, behavior::ActionType::Destroy, std::get<behavior::HitActionState>(brain.m_data));
            return StateStep::Running;
        }
        return step(states::HitState::update(rc, behavior::ActionType::Destroy,
                                             std::get<behavior::HitActionState>(brain.m_data)));
    case RoleBranch::Revive:
    case RoleBranch::Wake:
    case RoleBranch::GetUp:
    case RoleBranch::HitFloor:
    case RoleBranch::HitDown:
    case RoleBranch::HitUp:
    case RoleBranch::HitSwitch:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
        return step(states::HitState::update(rc, action, std::get<behavior::HitActionState>(brain.m_data)));
    case RoleBranch::Jostled:
        return step(states::Jostled::update(rc, action, std::get<behavior::JostledState>(brain.m_data)));
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        return step(states::AiMove::update(rc, action, std::get<ai::ActionMove>(brain.m_data)));
    case RoleBranch::Finding:
        return step(states::Finding::update(rc, action, std::get<behavior::ActionState>(brain.m_data)));
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        return step(states::Locomotion::update(rc, action, std::get<behavior::ActionState>(brain.m_data)));
    case RoleBranch::Attack:
        return states::CastState::update(brain, world, entity);
    }
    return StateStep::Done;
}
// 退出当前分支的叶子。
void exitState(RoleBrain& brain, World& world, Entity entity)
{
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(brain.m_branch));
    const auto action = branchAction(brain.m_branch);
    switch (brain.m_branch)
    {
    case RoleBranch::Death:
    case RoleBranch::Revive:
    case RoleBranch::Wake:
    case RoleBranch::GetUp:
    case RoleBranch::HitFloor:
    case RoleBranch::HitDown:
    case RoleBranch::HitUp:
    case RoleBranch::HitSwitch:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
        if (brain.m_destroyStage == 0)
            states::HitState::exit(rc, action, std::get<behavior::HitActionState>(brain.m_data));
        else
            states::HitState::exit(rc, behavior::ActionType::Destroy, std::get<behavior::HitActionState>(brain.m_data));
        // 退出未清监听（非 Death/落地 Hit）时，直接清除位移监听，避免遗留事件路由。
        if (world.ecs().get<Displacement>(entity).listenerNode == static_cast<int32_t>(brain.m_branch))
            world.ecs().get<Displacement>(entity).listenerNode = -1;
        break;
    case RoleBranch::Jostled:
        states::Jostled::exit(rc, action, std::get<behavior::JostledState>(brain.m_data));
        break;
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        states::AiMove::exit(rc, action, std::get<ai::ActionMove>(brain.m_data));
        break;
    case RoleBranch::Finding:
        states::Finding::exit(rc, action, std::get<behavior::ActionState>(brain.m_data));
        break;
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        states::Locomotion::exit(rc, action, std::get<behavior::ActionState>(brain.m_data));
        break;
    case RoleBranch::Attack:
        states::CastState::exit(brain, world, entity);
        break;
    }
}
// 尝试进入指定分支；守卫失败不触发任何退出调用，进入失败（仅 Attack）补一次守卫退出。
bool tryEnter(RoleBrain& brain, RoleBranch branch, World& world, Entity entity, bool city)
{
    if (!guardHolds(branch, world, entity, city))
        return false;
    brain.m_branch = branch;
    resetData(brain);
    if (!enterState(brain, world, entity))
    {
        exitGuard(branch, world, entity);
        return false;
    }
    brain.m_phase        = BrainPhase::Running;
    brain.m_noBranchLogged = false;
    return true;
}
// 分派位移事件到当前分支的叶子。
bool dispatchDisplacement(RoleBrain& brain, World& world, Entity entity, DisplacementEvent event)
{
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(brain.m_branch));
    rc.setEvent(event);
    const auto action = branchAction(brain.m_branch);
    switch (brain.m_branch)
    {
    case RoleBranch::Death:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
    case RoleBranch::HitUp:
    case RoleBranch::HitDown:
    case RoleBranch::HitFloor:
        return states::HitState::onDisplacement(rc, action, std::get<behavior::HitActionState>(brain.m_data));
    case RoleBranch::Jostled:
        return states::Jostled::onDisplacement(rc, action, std::get<behavior::JostledState>(brain.m_data));
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        return states::AiMove::onDisplacement(rc, action, std::get<ai::ActionMove>(brain.m_data));
    case RoleBranch::Attack:
        return states::CastState::onDisplacement(rc, brain.m_cast);
    default:
        return false;
    }
}
// 无可进入分支时退回待机并只记录一次错误日志。
bool fallbackToIdle(RoleBrain& brain, World& world, Entity entity, bool city)
{
    if (!brain.m_noBranchLogged)
    {
        MG_LOG_E("Role brain has no enterable branch; forcing Idle");
        brain.m_noBranchLogged = true;
    }
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Idle;
    return tryEnter(brain, RoleBranch::Idle, world, entity, city);
}
}  // namespace

bool enterFirst(RoleBrain& brain, World& world, Entity entity, bool city)
{
    const size_t count = city ? kTownBranchCount : kBranchCount;
    for (size_t i = 0; i < count; ++i)
    {
        // 城镇模板分支固定为 Finding、Idle。
        const RoleBranch branch = city ? (i == 0 ? RoleBranch::Finding : RoleBranch::Idle)
                                       : static_cast<RoleBranch>(i);
        if (tryEnter(brain, branch, world, entity, city))
            return true;
    }
    return fallbackToIdle(brain, world, entity, city);
}

void update(RoleBrain& brain, World& world, Entity entity, bool city)
{
    if (brain.m_phase == BrainPhase::NotEntered)
    {
        if (!enterFirst(brain, world, entity, city))
            return;
    }
    if (brain.m_phase != BrainPhase::Running)
        return;
    // 仅重查当前分支守卫（Selector 非抢占）。
    if (guardHolds(brain.m_branch, world, entity, city) && updateState(brain, world, entity) == StateStep::Running)
        return;
    // 守卫失效或叶子完成：先退叶子再退守卫，然后循环选择下一分支（仅进入，本帧不更新）。
    exitState(brain, world, entity);
    exitGuard(brain.m_branch, world, entity);
    const size_t count = city ? kTownBranchCount : kBranchCount;
    const auto current = static_cast<size_t>(brain.m_branch);
    bool entered       = false;
    for (size_t k = 1; k <= count; ++k)
    {
        const size_t idx = (current + k) % count;
        const RoleBranch branch = city ? (idx == 0 ? RoleBranch::Finding : RoleBranch::Idle)
                                       : static_cast<RoleBranch>(idx);
        if (tryEnter(brain, branch, world, entity, city))
        {
            entered = true;
            break;
        }
    }
    if (!entered)
        fallbackToIdle(brain, world, entity, city);
}

void exitBranch(RoleBrain& brain, World& world, Entity entity)
{
    exitState(brain, world, entity);
    exitGuard(brain.m_branch, world, entity);
    brain.m_phase = BrainPhase::NotEntered;
}

bool RoleContext::onDisplacementEvent(DisplacementEvent event)
{
    auto& brain        = m_world.ecs().get<RoleBrain>(m_entity);
    const int32_t node = m_world.ecs().get<Displacement>(m_entity).listenerNode;
    if (node < 0)
        return false;
    if (static_cast<RoleBranch>(node) == brain.m_branch)
        return dispatchDisplacement(brain, m_world, m_entity, event);
    return false;
}
}  // namespace mugen::sim::role::brain
