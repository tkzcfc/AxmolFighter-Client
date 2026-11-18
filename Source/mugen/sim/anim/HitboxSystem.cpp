// [6.2] HitboxSystem
// 读：`MotionStateComponent`、`TransformComponent`、avatar 资源
// 写：`HitboxesComponent`（攻击框、受击框）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/anim/HitboxRules.h"
#include "mugen/avatar/MotionPlayer.h"

namespace mugen::sim
{

// 系统 HitboxSystem：6.2 行，见 DESIGN.md §3.2。采样当前动作的碰撞框。
void runHitbox(World& world)
{
    world.ecs().each<MotionStateComponent, HitboxesComponent, TransformComponent>(
        [&](Entity entity, const MotionStateComponent& motion, HitboxesComponent& hitboxes, const TransformComponent& transform) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            const auto& identity = world.ecs().get<IdentityComponent>(entity);
            if (identity.born.frame == world.frame() && identity.born.stage > SpawnStage::RoleTimeline)
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->paused)
                    return;
            hitboxes.attack.clear();
            hitboxes.damage.clear();
            hitboxes.sampledFrame = world.frame();
            MotionPlayer player;
            if (!motion.motionFile.empty() && !motion.motionName.empty() && player.bind(motion.motionFile) &&
                player.play(motion.motionName, motion.entryId, motion.loop))
            {
                player.seek(static_cast<int32_t>(motion.timeMs.toInt()));
                std::vector<const DamageBox*> attack, damage;
                player.boxesAt(attack, damage);
                for (const auto* box : attack)
                    hitboxes.attack.push_back(placeCombatBox(*box, transform, hitboxes.depth));
                for (const auto* box : damage)
                    hitboxes.damage.push_back(placeCombatBox(*box, transform, hitboxes.depth));
            }
            // 房间实体（传送门/物品/障碍）在无动作文件或动作无受击框时退回配置半径盒。
            if (hitboxes.damage.empty() && hitboxes.radius > Fixed{} &&
                (identity.kind == SpawnKind::Portal || identity.kind == SpawnKind::Goods ||
                 identity.kind == SpawnKind::Obstacle))
            {
                const Fixed radius = hitboxes.radius;
                const Fixed span   = radius * Fixed::fromInt(2);
                hitboxes.damage.push_back(
                    WorldBox{{transform.position.x - radius, transform.position.z - radius, transform.position.y},
                             {span, span, span}});
            }
        });
}
}  // namespace mugen::sim
