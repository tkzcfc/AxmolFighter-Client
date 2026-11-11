// [6.2] HitboxSystem
// 读：`MotionState`、`Transform`、avatar 资源
// 写：`Hitboxes`（攻击框、受击框）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/avatar/MotionPlayer.h"

namespace mugen::sim
{
namespace
{
// 按朝向和缩放把局部伤害框转换为世界框（移植旧实现 RoleActor::transformBox）。
WorldBox transformBox(const DamageBox& box, const Transform& t)
{
    WorldBox result;
    result.m_size    = {Fixed::fromInt(box.size.x) * t.m_scale, Fixed::fromInt(box.size.y) * t.m_scale,
                        Fixed::fromInt(box.size.z) * t.m_scale};
    const Fixed localX = Fixed::fromInt(box.pos.x) * t.m_scale;
    result.m_pos       = {t.m_position.x + (t.m_towardX == -1 ? -localX - result.m_size.x : localX),
                          Fixed::fromInt(box.pos.y) * t.m_scale,
                          t.m_position.y + t.m_position.z + Fixed::fromInt(box.pos.z) * t.m_scale};
    return result;
}
}  // namespace

// 系统 HitboxSystem：6.2 行，见 DESIGN.md §3.2。采样当前动作的碰撞框。
void runHitbox(World& world)
{
    world.ecs().each<MotionState, Hitboxes, Transform>(
        [&](Entity entity, const MotionState& motion, Hitboxes& hitboxes, const Transform& transform) {
            if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
                return;
            if (const Pause* pause = world.ecs().tryGet<Pause>(entity))
                if (pause->m_paused)
                    return;
            hitboxes.m_attack.clear();
            hitboxes.m_damage.clear();
            hitboxes.m_sampledFrame = world.frame();
            if (motion.m_motionFile.empty() || motion.m_motionName.empty())
                return;
            MotionPlayer player;
            if (!player.bind(motion.m_motionFile) || !player.play(motion.m_motionName, motion.m_entryId, motion.m_loop))
                return;
            player.seek(static_cast<int32_t>(motion.m_timeMs.toInt()));
            std::vector<const DamageBox*> attack, damage;
            player.boxesAt(attack, damage);
            for (const auto* box : attack)
                hitboxes.m_attack.push_back(transformBox(*box, transform));
            for (const auto* box : damage)
                hitboxes.m_damage.push_back(transformBox(*box, transform));
        });
}
}  // namespace mugen::sim
