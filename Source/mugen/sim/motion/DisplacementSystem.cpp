// [5.1] DisplacementSystem
// 读：`DisplacementComponent`、`TransformComponent`、`StatsComponent`、`StatusFlagsComponent`、`MapRegion`
// 写：`TransformComponent`、`DisplacementComponent`（积分、事件位、内建策略反应）
// 消费：—
// 产生：`DisplacementEvent`（曲线耗尽时经 displacement::rules::settle）
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/battle/ClipDef.h"

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
    const Fixed dtMs   = world.ecs().singleton<WorldClock>().dtMs;
    const auto& region = world.ecs().singleton<MapRegion>();
    world.ecs().each<TransformComponent, DisplacementComponent, HitboxesComponent>(
        [&](Entity entity, TransformComponent& transform, DisplacementComponent& d, const HitboxesComponent& hitboxes) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            const auto& born = world.ecs().get<IdentityComponent>(entity).born;
            if (born.frame == world.frame() && born.stage > SpawnStage::RoleTimeline)
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->paused)
                    return;
            const auto last   = transform.position;
            auto& velocity    = d.velocity;
            const auto* roleState = world.ecs().tryGet<RoleStateComponent>(entity);
            const bool moving = roleState && role::isMoveState(roleState->state) &&
                                 roleState->state != RoleStateId::Attack;
            const auto* status = world.ecs().tryGet<StatusFlagsComponent>(entity);
            const bool cannotMove = status && !status->canMove();

            auto v = velocity;
            v.x *= moving || cannotMove ? d.velocityScale.x : d.defaultVelocityScale;
            v.y *= moving ? d.velocityScale.y : d.defaultVelocityScale;
            v.z *= moving || cannotMove ? d.velocityScale.z : d.defaultVelocityScale;
            if (cannotMove && v.y > Fixed{})
                v.y = {};
            const Fixed dt = dtMs * d.extraTimeScale;
            d.elapsedMs += dt;
            const auto x = displacement::integrate(
                d.elapsedMs, v.x,
                transform.vectorX * d.acceleration.x * (cannotMove ? d.velocityScale.x : Fixed::fromInt(1)),
                d.accelerationTime.x, d.velocityTime.x, dt);
            const bool grounded = transform.position.y == Fixed{} && v.y == Fixed{} && d.acceleration.y == Fixed{};
            const auto y        = displacement::integrate(d.elapsedMs, v.y, d.acceleration.y, d.accelerationTime.y,
                                                          d.velocityTime.y, dt, grounded ? Fixed{} : d.gravity * conf::kGravity);
            const auto z = displacement::integrate(
                d.elapsedMs, v.z,
                transform.vectorZ * d.acceleration.z * (cannotMove ? d.velocityScale.z : Fixed::fromInt(1)),
                d.accelerationTime.z, d.velocityTime.z, dt);
            velocity     = {x.velocity, y.velocity, z.velocity};
            d.offset   = {x.offset, y.offset, z.offset * conf::kZRate};
            const auto offset = d.offset;

            // 事件位（R11）：下一帧由状态体/FSM 读取，本帧只记录。
            d.brake = d.air = d.floor = d.lie = d.wall = false;
            if (velocity.x == Fixed{} && velocity.y == Fixed{} && velocity.z == Fixed{})
                d.brake = true;
            transform.position.x += offset.x;
            transform.position.y += offset.y;
            transform.position.z += offset.z;
            if (transform.position.y > Fixed{})
            {
                if (y.velocity < Fixed{})
                    d.air = true;
            }
            else if (transform.position.y < Fixed{})
            {
                transform.position.y = {};
                if (y.velocity < Fixed{})
                {
                    if (y.velocity < -conf::kBounces && d.bounces > Fixed{})
                    {
                        d.floor = true;
                        // EndClip/Stop 或浮空值耗尽：清速度不反弹（R11 当帧策略反应）。
                        if (d.floorPolicy != 0 || d.noFloorBounce)
                            velocity = {};
                        else
                            velocity.y = -y.velocity * d.bounces;
                    }
                    else
                    {
                        d.lie  = true;
                        velocity = {};
                    }
                }
            }
            else if (y.velocity == Fixed{})
                d.lie = true;

            const Fixed radius = hitboxes.radius;
            // 地图边界钳制（移植旧实现 DisplacementDriver::update）。
            // obstruct=0 / Pass：墙事件被消费且不贴墙，弹道可飞出角色可走区域。
            const auto regionX = [&]() {
                const FixedVec2 point{transform.position.x + signFixed(offset.x) * radius, transform.position.z};
                if (offset.x < Fixed{})
                {
                    const Fixed bound = region.getLeftRegion(point);
                    if (point.x != bound)
                    {
                        d.wall          = true;
                        transform.position.x = bound + radius;
                    }
                }
                else if (offset.x > Fixed{})
                {
                    const Fixed bound = region.getRightRegion(point);
                    if (point.x != bound)
                    {
                        d.wall          = true;
                        transform.position.x = bound - radius;
                    }
                }
            };
            const auto regionZ = [&]() {
                const FixedVec2 point{transform.position.x, transform.position.z + signFixed(offset.z) * radius};
                if (offset.z < Fixed{})
                {
                    const Fixed bound = region.getBottomRegion(point);
                    if (point.y != bound)
                    {
                        d.wall          = true;
                        transform.position.z = bound + radius;
                    }
                }
                else if (offset.z > Fixed{})
                {
                    const Fixed bound = region.getTopRegion(point);
                    if (point.y != bound)
                    {
                        d.wall          = true;
                        transform.position.z = bound - radius;
                    }
                }
            };
            if (!region.moveRanges.empty() &&
                d.wallPolicy != static_cast<int32_t>(conf::battle::ClipWallPolicy::Pass))
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
            d.offset = {transform.position.x - last.x, transform.position.y - last.y,
                          transform.position.z - last.z};
            displacement::rules::settle(world, entity);
        });
}
}  // namespace mugen::sim
