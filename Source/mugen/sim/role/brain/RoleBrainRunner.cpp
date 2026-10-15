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
void RoleBrainDriver::_resetData()
{
    m_brain.m_destroyStage = 0;
    switch (m_brain.m_branch)
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
        m_brain.m_data = behavior::HitActionState{};
        break;
    case RoleBranch::Jostled:
        m_brain.m_data = behavior::JostledState{};
        break;
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        m_brain.m_data = ai::ActionMove{};
        break;
    case RoleBranch::Finding:
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        m_brain.m_data = behavior::ActionState{};
        break;
    case RoleBranch::Attack:
        break;
    }
}
// 进入当前分支的叶子；返回是否成功（仅 Attack 可能失败）。
bool RoleBrainDriver::_enterState()
{
    RoleContext rc{m_world, m_entity};
    rc.setNode(static_cast<int32_t>(m_brain.m_branch));
    const auto action = _branchAction(m_brain.m_branch);
    switch (m_brain.m_branch)
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
        states::HitState::enter(rc, action, std::get<behavior::HitActionState>(m_brain.m_data));
        return true;
    case RoleBranch::Jostled:
        states::Jostled::enter(rc, action, std::get<behavior::JostledState>(m_brain.m_data));
        return true;
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        states::AiMove::enter(rc, action, std::get<ai::ActionMove>(m_brain.m_data));
        return true;
    case RoleBranch::Finding:
        states::Finding::enter(rc, action, std::get<behavior::ActionState>(m_brain.m_data));
        return true;
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        states::Locomotion::enter(rc, action, std::get<behavior::ActionState>(m_brain.m_data));
        return true;
    case RoleBranch::Attack:
        return states::CastState::enter(m_brain, m_world, m_entity);
    }
    return false;
}
// 推进当前分支的叶子；Death 分支按两段（Death 后 Destroy）推进。
StateStep RoleBrainDriver::_updateState()
{
    RoleContext rc{m_world, m_entity};
    rc.setNode(static_cast<int32_t>(m_brain.m_branch));
    const auto action = _branchAction(m_brain.m_branch);
    const auto step   = [](BTStatus status) { return status == BTStatus::Running ? StateStep::Running : StateStep::Done; };
    switch (m_brain.m_branch)
    {
    case RoleBranch::Death:
        if (m_brain.m_destroyStage == 0)
        {
            if (states::HitState::update(rc, action, std::get<behavior::HitActionState>(m_brain.m_data)) == BTStatus::Running)
                return StateStep::Running;
            m_brain.m_destroyStage = 1;
            states::HitState::enter(rc, behavior::ActionType::Destroy, std::get<behavior::HitActionState>(m_brain.m_data));
            return StateStep::Running;
        }
        return step(states::HitState::update(rc, behavior::ActionType::Destroy,
                                             std::get<behavior::HitActionState>(m_brain.m_data)));
    case RoleBranch::Revive:
    case RoleBranch::Wake:
    case RoleBranch::GetUp:
    case RoleBranch::HitFloor:
    case RoleBranch::HitDown:
    case RoleBranch::HitUp:
    case RoleBranch::HitSwitch:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
        return step(states::HitState::update(rc, action, std::get<behavior::HitActionState>(m_brain.m_data)));
    case RoleBranch::Jostled:
        return step(states::Jostled::update(rc, action, std::get<behavior::JostledState>(m_brain.m_data)));
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        return step(states::AiMove::update(rc, action, std::get<ai::ActionMove>(m_brain.m_data)));
    case RoleBranch::Finding:
        return step(states::Finding::update(rc, action, std::get<behavior::ActionState>(m_brain.m_data)));
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        return step(states::Locomotion::update(rc, action, std::get<behavior::ActionState>(m_brain.m_data)));
    case RoleBranch::Attack:
        return states::CastState::update(m_brain, m_world, m_entity);
    }
    return StateStep::Done;
}
// 退出当前分支的叶子。
void RoleBrainDriver::_exitState()
{
    RoleContext rc{m_world, m_entity};
    rc.setNode(static_cast<int32_t>(m_brain.m_branch));
    const auto action = _branchAction(m_brain.m_branch);
    switch (m_brain.m_branch)
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
        if (m_brain.m_destroyStage == 0)
            states::HitState::exit(rc, action, std::get<behavior::HitActionState>(m_brain.m_data));
        else
            states::HitState::exit(rc, behavior::ActionType::Destroy, std::get<behavior::HitActionState>(m_brain.m_data));
        // 退出未清监听（非 Death/落地 Hit）时，直接清除位移监听，避免遗留事件路由。
        if (m_world.ecs().get<Displacement>(m_entity).listenerNode == static_cast<int32_t>(m_brain.m_branch))
            m_world.ecs().get<Displacement>(m_entity).listenerNode = -1;
        break;
    case RoleBranch::Jostled:
        states::Jostled::exit(rc, action, std::get<behavior::JostledState>(m_brain.m_data));
        break;
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        states::AiMove::exit(rc, action, std::get<ai::ActionMove>(m_brain.m_data));
        break;
    case RoleBranch::Finding:
        states::Finding::exit(rc, action, std::get<behavior::ActionState>(m_brain.m_data));
        break;
    case RoleBranch::Run:
    case RoleBranch::Walk:
    case RoleBranch::Idle:
        states::Locomotion::exit(rc, action, std::get<behavior::ActionState>(m_brain.m_data));
        break;
    case RoleBranch::Attack:
        states::CastState::exit(m_brain, m_world, m_entity);
        break;
    }
}
// 尝试进入指定分支；守卫失败不触发任何退出调用，进入失败（仅 Attack）补一次守卫退出。
bool RoleBrainDriver::_tryEnter(RoleBranch branch)
{
    if (!_guardHolds(branch))
        return false;
    m_brain.m_branch = branch;
    _resetData();
    if (!_enterState())
    {
        _exitGuard(branch);
        return false;
    }
    m_brain.m_phase        = BrainPhase::Running;
    m_brain.m_noBranchLogged = false;
    return true;
}
bool RoleBrainDriver::onDisplacement(DisplacementEvent event)
{
    RoleContext rc{m_world, m_entity};
    rc.setNode(static_cast<int32_t>(m_brain.m_branch));
    rc.setEvent(event);
    const auto action = _branchAction(m_brain.m_branch);
    switch (m_brain.m_branch)
    {
    case RoleBranch::Death:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
    case RoleBranch::HitUp:
    case RoleBranch::HitDown:
    case RoleBranch::HitFloor:
        return states::HitState::onDisplacement(rc, action, std::get<behavior::HitActionState>(m_brain.m_data));
    case RoleBranch::Jostled:
        return states::Jostled::onDisplacement(rc, action, std::get<behavior::JostledState>(m_brain.m_data));
    case RoleBranch::Patrol:
    case RoleBranch::Chase:
    case RoleBranch::Alert:
        return states::AiMove::onDisplacement(rc, action, std::get<ai::ActionMove>(m_brain.m_data));
    case RoleBranch::Attack:
        return states::CastState::onDisplacement(rc, m_brain.m_cast);
    default:
        return false;
    }
}
// 无可进入分支时退回待机并只记录一次错误日志。
bool RoleBrainDriver::_fallbackToIdle()
{
    if (!m_brain.m_noBranchLogged)
    {
        MG_LOG_E("Role brain has no enterable branch; forcing Idle");
        m_brain.m_noBranchLogged = true;
    }
    m_world.ecs().get<RoleStatus>(m_entity).eEntityStatus = EntityRoleStatus::Idle;
    return _tryEnter(RoleBranch::Idle);
}

bool RoleBrainDriver::enterFirst()
{
    const size_t count = m_city ? kTownBranchCount : kBranchCount;
    for (size_t i = 0; i < count; ++i)
    {
        // 城镇模板分支固定为 Finding、Idle。
        const RoleBranch branch = m_city ? (i == 0 ? RoleBranch::Finding : RoleBranch::Idle)
                                       : static_cast<RoleBranch>(i);
        if (_tryEnter(branch))
            return true;
    }
    return _fallbackToIdle();
}

void RoleBrainDriver::update()
{
    if (m_brain.m_phase == BrainPhase::NotEntered)
    {
        if (!enterFirst())
            return;
    }
    if (m_brain.m_phase != BrainPhase::Running)
        return;
    // 仅重查当前分支守卫（Selector 非抢占）。
    if (_guardHolds(m_brain.m_branch) && _updateState() == StateStep::Running)
        return;
    // 守卫失效或叶子完成：先退叶子再退守卫，然后按优先级从零重选分支（仅进入，本帧不更新）。
    _exitState();
    _exitGuard(m_brain.m_branch);
    enterFirst();
}

void RoleBrainDriver::exitBranch()
{
    _exitState();
    _exitGuard(m_brain.m_branch);
    m_brain.m_phase = BrainPhase::NotEntered;
}

bool RoleContext::onDisplacementEvent(DisplacementEvent event)
{
    auto& brain        = m_world.ecs().get<RoleBrain>(m_entity);
    const int32_t node = m_world.ecs().get<Displacement>(m_entity).listenerNode;
    if (node < 0)
        return false;
    if (static_cast<RoleBranch>(node) == brain.m_branch)
        return RoleBrainDriver(m_world, m_entity, brain, false).onDisplacement(event);
    return false;
}
}  // namespace mugen::sim::role::brain
