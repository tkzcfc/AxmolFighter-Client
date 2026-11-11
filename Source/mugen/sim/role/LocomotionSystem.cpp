// [4.5] LocomotionSystem
// 读：`RoleState`、`Intent.m_move`、`Pause`
// 写：`Displacement`（控制速度）、`Transform`（朝向）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/FixedMath.h"

namespace mugen::sim
{
// 系统 LocomotionSystem：4.5 行，见 DESIGN.md §3.2。B1 只实现走跑控制速度。
void runLocomotion(World& world)
{
    world.ecs().each<RoleState, Intent, Transform, Displacement, Identity>(
        [&](Entity entity, const RoleState& state, const Intent& intent, Transform& transform,
            Displacement& displacement, const Identity& identity) {
            if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
                return;
            if (const Pause* pause = world.ecs().tryGet<Pause>(entity))
                if (pause->m_paused)
                    return;
            // 过渡帧不写速度（与旧实现一致：状态进入帧只播动作，不推进位移）。
            if (state.m_state != state.m_prev)
                return;
            if (state.m_state == RoleStateId::Idle)
            {
                displacement.m_velocity = {};
                return;
            }
            if (!role::isMoveState(state.m_state))
                return;
            const auto& move = intent.m_move;
            if (move.m_quadrant == ControlQuadrant::None)
            {
                displacement.m_velocity.x = {};
                displacement.m_velocity.z = {};
                return;
            }
            const Fixed angle = Fixed::ratio(move.m_angle, 1000);
            if (hasHorizontal(move.m_angle))
            {
                transform.m_towardX = vectorX(move.m_quadrant);
                transform.m_vectorX = Fixed::fromInt(transform.m_towardX);
            }
            if (hasVertical(move.m_angle))
                transform.m_vectorZ = Fixed::fromInt(vectorZ(move.m_quadrant));
            const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
            if (row)
            {
                const Fixed rate  = state.m_state == RoleStateId::Run ? conf::kRunRate : conf::kWalkRate;
                const Fixed speed = row->velocity * rate;
                // 移速修正只由 Displacement 乘 velocityScale，这里写原始速度。
                displacement.m_velocity.x = FixedMath::cosDeg(angle) * speed * transform.m_vectorX;
                displacement.m_velocity.z = FixedMath::sinDeg(angle) * speed * transform.m_vectorZ;
                displacement.m_velocity.y = {};
            }
        });
}
}  // namespace mugen::sim
