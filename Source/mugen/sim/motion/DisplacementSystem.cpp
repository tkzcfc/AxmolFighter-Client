// [5.1] DisplacementSystem
// 读：`DisplacementComponent`、`TransformComponent`、`StatsComponent`、`StatusFlagsComponent`、`MapRegion`
// 写：`TransformComponent`、`DisplacementComponent`（积分、事件位、内建策略反应）
// 消费：—
// 产生：`DisplacementEvent`（曲线耗尽时经 displacement::rules::settle）
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim
{
namespace
{
// 返回定点数绝对值。
Fixed absFixed(Fixed value)
{
    return value < Fixed{} ? -value : value;
}
// 返回定点数符号（-1/0/1）。
Fixed signFixed(Fixed value)
{
    return Fixed::fromInt(value > Fixed{} ? 1 : value < Fixed{} ? -1 : 0);
}
}  // namespace

// 系统 DisplacementSystem：5.1 行，见 DESIGN.md §3.2。
void runDisplacement(World& world)
{
    const Fixed dtMs   = world.ecs().singleton<WorldClock>().m_dtMs;
    const auto& region = world.ecs().singleton<MapRegion>();
    world.ecs().each<TransformComponent, DisplacementComponent, HitboxesComponent>(
        [&](Entity entity, TransformComponent& transform, DisplacementComponent& d, const HitboxesComponent& hitboxes) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->m_paused)
                    return;
            const auto last   = transform.m_position;
            auto& velocity    = d.m_velocity;
            const auto* roleState = world.ecs().tryGet<RoleStateComponent>(entity);
            const bool moving = roleState && role::isMoveState(roleState->m_state) &&
                                 roleState->m_state != RoleStateId::Attack;
            const auto* status = world.ecs().tryGet<StatusFlagsComponent>(entity);
            const bool cannotMove = status && !status->canMove();

            auto v = velocity;
            v.x *= moving || cannotMove ? d.m_velocityScale.x : d.m_defaultVelocityScale;
            v.y *= moving ? d.m_velocityScale.y : d.m_defaultVelocityScale;
            v.z *= moving || cannotMove ? d.m_velocityScale.z : d.m_defaultVelocityScale;
            if (cannotMove && v.y > Fixed{})
                v.y = {};
            const Fixed dt = dtMs * d.m_extraTimeScale;
            d.m_elapsedMs += dt;
            const auto x = displacement::integrate(
                d.m_elapsedMs, v.x,
                transform.m_vectorX * d.m_acceleration.x * (cannotMove ? d.m_velocityScale.x : Fixed::fromInt(1)),
                d.m_accelerationTime.x, d.m_velocityTime.x, dt);
            const bool grounded = transform.m_position.y == Fixed{} && v.y == Fixed{} && d.m_acceleration.y == Fixed{};
            const auto y        = displacement::integrate(d.m_elapsedMs, v.y, d.m_acceleration.y, d.m_accelerationTime.y,
                                                          d.m_velocityTime.y, dt, grounded ? Fixed{} : d.m_gravity * conf::kGravity);
            const auto z = displacement::integrate(
                d.m_elapsedMs, v.z,
                transform.m_vectorZ * d.m_acceleration.z * (cannotMove ? d.m_velocityScale.z : Fixed::fromInt(1)),
                d.m_accelerationTime.z, d.m_velocityTime.z, dt);
            velocity     = {x.m_velocity, y.m_velocity, z.m_velocity};
            d.m_offset   = {x.m_offset, y.m_offset, z.m_offset * conf::kZRate};
            const auto offset = d.m_offset;

            // 事件位（R11）：下一帧由状态体/FSM 读取，本帧只记录。
            d.m_brake = d.m_air = d.m_floor = d.m_lie = d.m_wall = false;
            if (velocity.x == Fixed{} && velocity.y == Fixed{} && velocity.z == Fixed{})
                d.m_brake = true;
            transform.m_position.x += offset.x;
            transform.m_position.y += offset.y;
            transform.m_position.z += offset.z;
            if (transform.m_position.y > Fixed{})
            {
                if (y.m_velocity < Fixed{})
                    d.m_air = true;
            }
            else if (transform.m_position.y < Fixed{})
            {
                transform.m_position.y = {};
                if (y.m_velocity < Fixed{})
                {
                    if (y.m_velocity < -conf::kBounces && d.m_bounces > Fixed{})
                    {
                        d.m_floor   = true;
                        velocity.y  = -y.m_velocity * d.m_bounces;
                    }
                    else
                    {
                        d.m_lie  = true;
                        velocity = {};
                    }
                }
            }
            else if (y.m_velocity == Fixed{})
                d.m_lie = true;

            const Fixed radius = hitboxes.m_radius;
            // 地图边界钳制（移植旧实现 DisplacementDriver::update）。
            const auto regionX = [&]() {
                const FixedVec2 point{transform.m_position.x + signFixed(offset.x) * radius, transform.m_position.z};
                if (offset.x < Fixed{})
                {
                    const Fixed bound = region.getLeftRegion(point);
                    if (point.x != bound)
                    {
                        d.m_wall          = true;
                        transform.m_position.x = bound + radius;
                    }
                }
                else if (offset.x > Fixed{})
                {
                    const Fixed bound = region.getRightRegion(point);
                    if (point.x != bound)
                    {
                        d.m_wall          = true;
                        transform.m_position.x = bound - radius;
                    }
                }
            };
            const auto regionZ = [&]() {
                const FixedVec2 point{transform.m_position.x, transform.m_position.z + signFixed(offset.z) * radius};
                if (offset.z < Fixed{})
                {
                    const Fixed bound = region.getBottomRegion(point);
                    if (point.y != bound)
                    {
                        d.m_wall          = true;
                        transform.m_position.z = bound + radius;
                    }
                }
                else if (offset.z > Fixed{})
                {
                    const Fixed bound = region.getTopRegion(point);
                    if (point.y != bound)
                    {
                        d.m_wall          = true;
                        transform.m_position.z = bound - radius;
                    }
                }
            };
            if (!region.m_moveRanges.empty())
            {
                if (absFixed(offset.z) < absFixed(offset.x))
                {
                    regionX();
                    regionZ();
                }
                else if (absFixed(offset.z) > absFixed(offset.x))
                {
                    regionZ();
                    regionX();
                }
            }
            d.m_offset = {transform.m_position.x - last.x, transform.m_position.y - last.y,
                          transform.m_position.z - last.z};
            displacement::rules::settle(world, entity);
        });
}
}  // namespace mugen::sim
