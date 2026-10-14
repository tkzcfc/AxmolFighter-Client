#include "RoleControl.h"

#include "mugen/sim/World.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/status/Status.h"

namespace mugen::sim::role::brain
{
void skillEvent(World& world, Entity entity, const PlayerCommand& command)
{
    auto* pool = world.ecs().tryGet<SkillPool>(entity);
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
    skill::dealButtonEvent(world, entity, command.controlEvent, command.pressType == 2);
}

void consumeControl(World& world, Entity entity, RoleControl& control)
{
    auto& queue = world.ecs().get<ControlEventQueue>(entity).commands;
    for (const auto& command : queue)
    {
        if (!world.ecs().get<ExtraState>(entity).isCanControl() ||
            world.ecs().get<RoleStatus>(entity).isJostled())
            continue;
        control.m_last = command;
        if (static_cast<int32_t>(command.controlEvent) >= static_cast<int32_t>(ControlEvent::CTR_ATKA))
            skillEvent(world, entity, command);
        else if (command.controlEvent == ControlEvent::CTR_IDLE)
            status::dealWithStatus(world, entity, EntityRoleStatus::Idle);
        else if (command.controlEvent == ControlEvent::CTR_RUN)
        {
            skill::interruptRun(world, entity);
            status::dealWithStatus(world, entity, EntityRoleStatus::Run);
        }
        else if (command.controlEvent == ControlEvent::CTR_WALK)
            status::dealWithStatus(world, entity, EntityRoleStatus::Walk);
    }
    queue.clear();
}
}  // namespace mugen::sim::role::brain
