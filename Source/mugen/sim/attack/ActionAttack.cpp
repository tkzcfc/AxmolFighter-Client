#include "ActionAttack.h"
#include "AttackSeams.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/FixedMath.h"

namespace mugen::sim::attack
{
/// 根据配置循环次数停止动画计时。
void completeAnimation(ActionAttack& action, const conf::ActionConfig& row, int64_t loops)
{
    if (!action.m_animationEnd && (row.loop == "1"_fx ||
        (row.loop != "-1"_fx && Fixed::fromInt(loops) >= row.loop)))
    {
        action.m_animationEnd = true;
        action.m_time = {};
    }
}
/// 等待动画结束后的配置延迟。
void dealWithTime(ActionAttack& action, const conf::ActionConfig& row)
{
    if (action.m_animationEnd && action.m_time >= Fixed::fromInt(row.actionDelayTime))
        action.m_finished = true;
}
/// 将控制象限转换为角色朝向和移动向量。
void dealWithDirection(World& world, Entity entity, bool facing)
{
    const auto& command = world.ecs().get<RoleTree>(entity).control;
    auto& transform = world.ecs().get<Transform>(entity);
    if (command.quadrant == 0)
        return;
    if (command.angle != 90000)
    {
        transform.vectorX = command.quadrant <= 2 ? "1"_fx : "-1"_fx;
        if (facing)
            transform.towardX = static_cast<int32_t>(transform.vectorX.toInt());
    }
    if (command.angle != 0)
        transform.vectorZ = command.quadrant == 1 || command.quadrant == 4 ? "1"_fx : "-1"_fx;
}
/// 按控制类型驱动位移或直接速度。
void dealWithControl(World& world, Entity entity, const conf::ActionConfig& row)
{
    auto& ecs = world.ecs();
    if (row.control == 2 && row.displacementId != -1)
        dealWithDirection(world, entity, true);
    else if ((row.control == 0 || row.control == 3) && row.displacementId == -1)
    {
        dealWithDirection(world, entity, row.control == 0);
        const auto& command = ecs.get<RoleTree>(entity).control;
        const auto& t = ecs.get<Transform>(entity);
        auto& velocity = ecs.get<RoleHitState>(entity).forceVelocity;
        const auto angle = Fixed::ratio(command.angle, 1000);
        velocity.x = command.quadrant == 0 ? Fixed{} : FixedMath::cosDeg(angle) * row.controlVelocity * t.vectorX;
        velocity.z = command.quadrant == 0 ? Fixed{} : FixedMath::sinDeg(angle) * row.controlVelocity * t.vectorZ;
    }
}
/// 在到达各镜头帧时发出一次震动事件。
void dealWithShake(World& world, Entity entity, ActionAttack& action, const conf::ActionConfig& row)
{
    for (size_t i = 0; i < row.cameraFrame.size() && i < row.cameraId.size(); ++i)
        if (!action.m_shaken[i] && row.cameraId[i] != -1 &&
            action.m_time > action.m_interval * Fixed::fromInt(row.cameraFrame[i]))
        {
            action.m_shaken[i] = 1;
            const auto* camera = conf::Config::instance().find<conf::CameraConfig>(row.cameraId[i]);
            if (camera) world.emit(entity, CameraShake{Fixed::fromInt(camera->amplitudeY),
                Fixed::fromInt(camera->duration), camera->id});
        }
}
/// 按障碍与落地配置决定是否结束动作或消费事件。
bool displacementEvent(World& world, Entity entity, ActionAttack& action,
                       const conf::ActionConfig& row, DisplacementEvent event)
{
    switch (event)
    {
    case DisplacementEvent::Brake: // 循环位移刹车后结束动作。
        if (row.loop == "-1"_fx && row.displacementId != -1) action.m_finished = true;
        break;
    case DisplacementEvent::Left: case DisplacementEvent::Right: // 四个边界碰撞共用障碍规则。
    case DisplacementEvent::Top: case DisplacementEvent::Bottom:
        if (row.obstruct == 0) return true;
        if (row.obstruct == 1)
        {
            world.ecs().get<RoleHitState>(entity).m_obstruct = true;
            action.m_finished = true;
        }
        break;
    case DisplacementEvent::Floor: case DisplacementEvent::Lie: // 落地与倒地共用地面规则。
        if (row.floor == 0) { action.m_finished = true; return true; }
        break;
    case DisplacementEvent::Air: break;
    }
    return false;
}
/// 清理动作结束时仍由本动作持有的效果。
void clearEffects(World& world, Entity entity, ActionAttack& action)
{
    effect::clearActionEffects(world, entity, action.m_effects);
    action.m_effects.clear();
}
}
