#include "RoleContext.h"

#include "mugen/sim/skill/SkillCaster.h"
#include <array>

namespace mugen::sim::role::brain
{
namespace
{
// 生命周期匹配使用动作类名，不使用可共享的动画名。
const char* behaviorName(behavior::ActionType type)
{
    constexpr std::array names{"ActionBase",     "ActionDeath",    "ActionGetUp",     "ActionHitFloor", "ActionHitDown",
                               "ActionHitUp",    "ActionHitSwitch", "ActionHit",      "ActionFall",     "ActionPatrol",
                               "ActionChase",    "ActionAlert",     "ActionRun",      "ActionWalk",     "ActionIdle",
                               "ActionSpurt",    "ActionCollect",   "ActionActive",   "ActionTransfer", "ActionMove",
                               "ActionReadyIdle", "ActionReadyMove", "ActionJostled", "ActionFollow",   "ActionSpecIdle",
                               "ActionControlled", "ActionWake",     "ActionRevive",    "ActionDestroy"};
    return names.at(static_cast<size_t>(type));
}
}  // namespace

// 将普通动作开始通知转交给世界拥有的 Buff 接缝。
void RoleContext::notifyActionStart(behavior::ActionType type)
{
    if (m_world.hitHooks().m_behaviorState)
        m_world.hitHooks().m_behaviorState(m_world, m_entity, BFEvent::BehaviorStateStart, behaviorName(type));
}

// 将普通动作结束通知转交给世界拥有的 Buff 接缝。
void RoleContext::notifyActionEnd(behavior::ActionType type)
{
    if (m_world.hitHooks().m_behaviorState)
        m_world.hitHooks().m_behaviorState(m_world, m_entity, BFEvent::BehaviorStateEnd, behaviorName(type));
}

// 将技能控制命令转换为方向角并交给技能系统。
void RoleContext::_skillEvent(const PlayerCommand& command)
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool)
        return;
    if (command.quadrant == 0)
        pool->m_controlAngle.reset();
    else
    {
        const auto angle     = Fixed::ratio(command.angle, 1000);
        pool->m_controlAngle = command.quadrant == 1   ? "90"_fx - angle
                               : command.quadrant == 2 ? "90"_fx + angle
                               : command.quadrant == 3 ? "270"_fx - angle
                                                       : "270"_fx + angle;
    }
    skill::SkillCaster(m_world, m_entity).dealButtonEvent(command.controlEvent, command.pressType == 2);
}

// 消费本帧控制命令并写入 RoleControl.m_last。
void RoleContext::consumeControl()
{
    auto& ecs   = m_world.ecs();
    auto& queue = ecs.get<ControlEventQueue>(m_entity).commands;
    for (const auto& command : queue)
    {
        if (!ecs.get<ExtraState>(m_entity).isCanControl() || ecs.get<RoleStatus>(m_entity).isJostled())
            continue;
        ecs.get<RoleControl>(m_entity).m_last = command;
        if (static_cast<int32_t>(command.controlEvent) >= static_cast<int32_t>(ControlEvent::CTR_ATKA))
            _skillEvent(command);
        else if (command.controlEvent == ControlEvent::CTR_IDLE)
            Role(m_world, m_entity).changeStatus(EntityRoleStatus::Idle);
        else if (command.controlEvent == ControlEvent::CTR_RUN)
        {
            skill::SkillCaster(m_world, m_entity).interruptRun();
            Role(m_world, m_entity).changeStatus(EntityRoleStatus::Run);
        }
        else if (command.controlEvent == ControlEvent::CTR_WALK)
            Role(m_world, m_entity).changeStatus(EntityRoleStatus::Walk);
    }
    queue.clear();
}
}  // namespace mugen::sim::role::brain
