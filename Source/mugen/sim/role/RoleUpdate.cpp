#include "RoleUpdate.h"
#include "mugen/sim/World.h"
#include "mugen/avatar/MotionPlayer.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/sim/skill/SkillCaster.h"
#include "mugen/sim/attack/AttackRole.h"
#include "mugen/sim/role/brain/states/HitStates.h"
#include "mugen/sim/role/brain/RoleBrainRunner.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/buff/BuffPool.h"
#include "mugen/sim/ability/AbilityPool.h"
#include <algorithm>

namespace mugen::sim::role
{
namespace
{
// 推进角色技能池。
void skillPoolUpdate(World& world, Entity entity) { skill::SkillCaster(world, entity).update(); }
// 推进角色 Buff 池。
void buffPoolUpdate(World& world, Entity entity) { buff::update(world, entity); }
// 推进角色能力池。
void abilityPoolUpdate(World& world, Entity entity) { ability::update(world, entity); }
// 推进伤害数字连击流程。
void hurtNumberComboUpdate(World&, Entity) {}
// 推进召唤流程。
void summonUpdate(World& world, Entity entity)
{
    auto& remaining = world.ecs().get<RoleTimers>(entity).summonMs;
    if (remaining < Fixed{}) return;
    remaining -= world.ecs().singleton<WorldClock>().dtMs;
    if (remaining <= Fixed{})
    {
        remaining = "-1"_fx;
        world.ecs().get<Vitals>(entity).hp = {};
        Role(world, entity).changeStatus(EntityRoleStatus::Death);
    }
}
// 推进角色狂暴技能流程。
void crazyUpdate(World& world, Entity entity) { skill::SkillCaster(world, entity).updateCrazy(); }
// 推进怒气流程。
void rageUpdate(World&, Entity) {}
// 推进受击保护流程。
void hitProtectUpdate(World& world, Entity entity) { Role(world, entity).updateHitProtect(); }
// 推进 AI 决策流程。驱动关闭时怪物不再做决策。
void aiUpdate(World& world, Entity entity)
{
    if (!world.setup().m_aiEnabled)
        return;
    ai::update(world, entity);
}
// 消费本帧控制命令。
void controlEventsUpdate(World& world, Entity entity) { role::brain::consumeControl(world, entity); }
// 推进角色状态机（房间切换首帧先进入再更新一次）。
void behaviorTreeUpdate(World& world, Entity entity)
{
    auto& brain = world.ecs().get<RoleBrain>(entity);
    role::brain::update(brain, world, entity, world.setup().mode == WorldMode::Town);
}
// 推进角色位移。
void displacementUpdate(World& world, Entity entity) { displacement::update(world, entity); }
// 更新角色碰撞体流程。
void collisionBodyUpdate(World&, Entity) {}
// 推进动作时间并重新登记本帧碰撞框。
void motionUpdate(World& world, Entity entity)
{
    auto& motion = world.ecs().get<MotionState>(entity);
    if (!motion.playing)
        return;
    MotionPlayer player;
    if (!player.bind(motion.motionFile) || !player.play(motion.motionName, motion.entryId, motion.loop))
        return;
    const Fixed duration = Fixed::fromInt(player.getDurationMs());
    motion.timeMs += world.ecs().singleton<WorldClock>().dtMs * motion.actionScale;
    if (duration > Fixed{})
    {
        if (motion.timeMs >= duration && (motion.loop || motion.m_completedLoops == 0))
        {
            motion.m_completedLoops += motion.loop ? motion.timeMs.raw / duration.raw : 1;
            if (world.ecs().has<EffectState>(entity))
                effect::animationComplete(world, entity, motion.m_completedLoops);
            else
            {
                attack::animationComplete(world, entity, motion.m_completedLoops);
                role::brain::states::animationComplete(world, entity);
            }
        }
        motion.timeMs =
            motion.loop ? Fixed::fromRaw(motion.timeMs.raw % duration.raw) : std::min(motion.timeMs, duration);
    }
    player.seek(static_cast<int32_t>(motion.timeMs.toInt()));
    std::vector<const DamageBox*> attack, damage;
    player.boxesAt(attack, damage);
    auto& collider        = world.ecs().get<ColliderState>(entity);
    const auto& transform = world.ecs().get<Transform>(entity);
    collider.attack.clear();
    collider.damage.clear();
    for (const auto* box : attack)
        collider.attack.push_back(transformBox(*box, transform));
    for (const auto* box : damage)
        collider.damage.push_back(transformBox(*box, transform));
}
}  // namespace
// 复用同一动画时间和碰撞框采样实现。
void updateMotion(World& world, Entity entity) { motionUpdate(world, entity); }
// 按朝向和缩放把局部伤害框转换为世界框。
WorldBox transformBox(const DamageBox& box, const Transform& t)
{
    WorldBox result;
    result.size        = {Fixed::fromInt(box.size.x) * t.scale, Fixed::fromInt(box.size.y) * t.scale,
                          Fixed::fromInt(box.size.z) * t.scale};
    const Fixed localX = Fixed::fromInt(box.pos.x) * t.scale;
    result.pos         = {t.position.x + (t.towardX == -1 ? -localX - result.size.x : localX),
                          Fixed::fromInt(box.pos.y) * t.scale,
                          t.position.y + t.position.z + Fixed::fromInt(box.pos.z) * t.scale};
    return result;
}
// 依固定顺序推进角色资源、状态、控制、行为、位移和动作。
void update(World& world, Entity entity)
{
    if (!world.ecs().has<RoleIdentity>(entity) || world.ecs().has<Puppet>(entity))
        return;
    skillPoolUpdate(world, entity);
    buffPoolUpdate(world, entity);
    abilityPoolUpdate(world, entity);
    hurtNumberComboUpdate(world, entity);
    auto& vitals   = world.ecs().get<Vitals>(entity);
    const Fixed dt = world.ecs().singleton<WorldClock>().dtMs;
    vitals.mp      = std::min(vitals.maxMp, vitals.mp + dt * conf::kMpIncreaseBasicValue * vitals.mpRecoveryScale);
    summonUpdate(world, entity);
    auto& timers    = world.ecs().get<RoleTimers>(entity);
    timers.freezeMs = std::max(Fixed{}, timers.freezeMs - dt);
    timers.staticMs = std::max(Fixed{}, timers.staticMs - dt);
    crazyUpdate(world, entity);
    rageUpdate(world, entity);
    hitProtectUpdate(world, entity);
    auto& sampledVitals = world.ecs().get<Vitals>(entity);
    sampledVitals.m_lostHp = sampledVitals.m_previousHp > sampledVitals.hp;
    sampledVitals.m_recoveredHp = sampledVitals.m_previousHp < sampledVitals.hp;
    sampledVitals.m_hpChangePercent = sampledVitals.hp == Fixed{} ? Fixed{} :
        (sampledVitals.hp - sampledVitals.m_previousHp) / sampledVitals.hp;
    sampledVitals.m_lostEp = sampledVitals.m_previousEp > sampledVitals.ep;
    sampledVitals.m_recoveredEp = sampledVitals.m_previousEp < sampledVitals.ep;
    sampledVitals.m_previousHp = sampledVitals.hp;
    sampledVitals.m_previousEp = sampledVitals.ep;
    const auto& sampledStatus = world.ecs().get<RoleStatus>(entity);
    const auto sampleSeconds = dt / "1000"_fx;
    timers.m_movedSeconds = sampledStatus.eEntityStatus == EntityRoleStatus::Run ?
        timers.m_movedSeconds + sampleSeconds : Fixed{};
    timers.m_standingSeconds = sampledStatus.eEntityStatus == EntityRoleStatus::Idle ?
        timers.m_standingSeconds + sampleSeconds : Fixed{};
    if (world.ecs().get<RoleIdentity>(entity).isAi && world.ecs().get<ExtraState>(entity).isCanControl())
        aiUpdate(world, entity);
    if (timers.freezeMs > Fixed{} || timers.staticMs > Fixed{})
        return;
    controlEventsUpdate(world, entity);
    behaviorTreeUpdate(world, entity);
    displacementUpdate(world, entity);
    motionUpdate(world, entity);
    collisionBodyUpdate(world, entity);
}
}  // namespace mugen::sim::role
