// [4.5] LocomotionSystem
// 读：`RoleStateComponent`、`IntentComponent.move`、`PauseComponent`
// 写：`DisplacementComponent`（控制速度）、`TransformComponent`（朝向）
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
    world.ecs().each<RoleStateComponent, IntentComponent, TransformComponent, DisplacementComponent, IdentityComponent>(
        [&](Entity entity, const RoleStateComponent& state, const IntentComponent& intent, TransformComponent& transform,
            DisplacementComponent& displacement, const IdentityComponent& identity) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (world.setup().mode == WorldMode::Town && !identity.playerSlot.has_value())
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->paused)
                    return;
            // 过渡帧不写速度（与旧实现一致：状态进入帧只播动作，不推进位移）。
            if (state.state != state.prev)
                return;
            if (state.state == RoleStateId::Idle)
            {
                // 只清水平速度；空中 y 交给重力积分，避免每帧抹掉落地速度。
                displacement.velocity.x = {};
                displacement.velocity.z = {};
                return;
            }
            if (!role::isMoveState(state.state))
                return;
            const auto& move = intent.move;
            if (move.quadrant == ControlQuadrant::None)
            {
                displacement.velocity.x = {};
                displacement.velocity.z = {};
                return;
            }
            const Fixed angle = Fixed::ratio(move.angle, 1000);
            if (hasHorizontal(move.angle))
            {
                transform.towardX = vectorX(move.quadrant);
                transform.vectorX = Fixed::fromInt(transform.towardX);
            }
            if (hasVertical(move.angle))
                transform.vectorZ = Fixed::fromInt(vectorZ(move.quadrant));
            const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
            if (row)
            {
                const Fixed rate  = state.state == RoleStateId::Run ? conf::kRunRate : conf::kWalkRate;
                const Fixed speed = row->velocity * rate;
                // 移速修正只由 DisplacementComponent 乘 velocityScale，这里写原始速度。
                displacement.velocity.x = FixedMath::cosDeg(angle) * speed * transform.vectorX;
                displacement.velocity.z = FixedMath::sinDeg(angle) * speed * transform.vectorZ;
                displacement.velocity.y = {};
            }
        });
}
}  // namespace mugen::sim
