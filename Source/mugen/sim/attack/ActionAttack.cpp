#include "ActionAttack.h"
#include "AttackSeam.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/FixedMath.h"

namespace mugen::sim::attack
{
/// 根据配置循环次数停止动画计时。
void ActionAttack::completeAnimation(const conf::ActionConfig& row, int64_t loops)
{
    if (!m_animationEnd && (row.loop == "1"_fx ||
        (row.loop != "-1"_fx && Fixed::fromInt(loops) >= row.loop)))
    {
        m_animationEnd = true;
        m_time = {};
    }
}
/// 等待动画结束后的配置延迟。
void ActionAttack::dealWithTime(const conf::ActionConfig& row)
{
    if (m_animationEnd && m_time >= Fixed::fromInt(row.actionDelayTime))
        m_finished = true;
}
// 构造动作攻击视图，只保存世界、实体句柄与动作状态引用。
ActionAttackDriver::ActionAttackDriver(World& world, Entity entity, ActionAttack& action)
    : m_world(world), m_entity(entity), m_action(action) {}
/// 将控制象限转换为角色朝向和移动向量。
void ActionAttackDriver::dealWithDirection(bool facing)
{
    const auto& command = m_world.ecs().get<RoleControl>(m_entity).m_last;
    auto& transform = m_world.ecs().get<Transform>(m_entity);
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
void ActionAttackDriver::dealWithControl(const conf::ActionConfig& row)
{
    auto& ecs = m_world.ecs();
    if (row.control == 2 && row.displacementId != -1)
        dealWithDirection(true);
    else if ((row.control == 0 || row.control == 3) && row.displacementId == -1)
    {
        dealWithDirection(row.control == 0);
        const auto& command = ecs.get<RoleControl>(m_entity).m_last;
        const auto& t = ecs.get<Transform>(m_entity);
        auto& velocity = ecs.get<RoleHitState>(m_entity).forceVelocity;
        const auto angle = Fixed::ratio(command.angle, 1000);
        velocity.x = command.quadrant == 0 ? Fixed{} : FixedMath::cosDeg(angle) * row.controlVelocity * t.vectorX;
        velocity.z = command.quadrant == 0 ? Fixed{} : FixedMath::sinDeg(angle) * row.controlVelocity * t.vectorZ;
    }
}
/// 在到达各镜头帧时发出一次震动事件。
void ActionAttackDriver::dealWithShake(const conf::ActionConfig& row)
{
    for (size_t i = 0; i < row.cameraFrame.size() && i < row.cameraId.size(); ++i)
        if (!m_action.m_shaken[i] && row.cameraId[i] != -1 &&
            m_action.m_time > m_action.m_interval * Fixed::fromInt(row.cameraFrame[i]))
        {
            m_action.m_shaken[i] = 1;
            const auto* camera = conf::Config::instance().find<conf::CameraConfig>(row.cameraId[i]);
            if (camera) m_world.emit(m_entity, CameraShake{Fixed::fromInt(camera->amplitudeY),
                Fixed::fromInt(camera->duration), camera->id});
        }
}
/// 按障碍与落地配置决定是否结束动作或消费事件。
bool ActionAttackDriver::onDisplacementEvent(const conf::ActionConfig& row, DisplacementEvent event)
{
    switch (event)
    {
    case DisplacementEvent::Brake: // 循环位移刹车后结束动作。
        if (row.loop == "-1"_fx && row.displacementId != -1) m_action.m_finished = true;
        break;
    case DisplacementEvent::Left: case DisplacementEvent::Right: // 四个边界碰撞共用障碍规则。
    case DisplacementEvent::Top: case DisplacementEvent::Bottom:
        if (row.obstruct == 0) return true;
        if (row.obstruct == 1)
        {
            m_world.ecs().get<RoleHitState>(m_entity).m_obstruct = true;
            m_action.m_finished = true;
        }
        break;
    case DisplacementEvent::Floor: case DisplacementEvent::Lie: // 落地与倒地共用地面规则。
        if (row.floor == 0) { m_action.m_finished = true; return true; }
        break;
    case DisplacementEvent::Air: break;
    }
    return false;
}
/// 清理动作结束时仍由本动作持有的效果。
void ActionAttackDriver::clearEffects()
{
    AttackSeam(m_world, m_entity).clearActionEffects(m_action.m_effects);
    m_action.m_effects.clear();
}
}
