// [4.3a] TimelineSystem(roles) / [4.3b] TimelineSystem(effects)
// 读：`ActionTimelineComponent`、`SkillCastComponent`、`RoleStateComponent`、`MotionStateComponent` 完成位、`EffectInfoComponent`、`DisplacementComponent` 事件位、`IntentComponent`、`WorldClock`
// 写：`ActionTimelineComponent`、`MotionStateComponent`、`DisplacementComponent`、`SkillCastComponent`、`TransformComponent`（片段控制朝向）
// 消费：—
// 产生：`CastEvent`、`BuffEvent`、`DespawnRequest`、`TransformRequest`；后继特效由 DespawnSystem 派生；位移事实由 displacement::rules 产生
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/combat/CombatRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/role/RoleFsmRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/ClipDef.h"

namespace mugen::sim
{
namespace
{
// 位移策略在 P5 当帧写事件位，时间轴下一帧读取（R11）。
bool displacementEndsClip(const DisplacementComponent& disp)
{
    if (disp.m_endOnBrake && disp.m_brake)
        return true;
    if (disp.m_floorPolicy == static_cast<int32_t>(conf::battle::ClipFloorPolicy::EndClip) &&
        (disp.m_floor || disp.m_lie))
        return true;
    if (disp.m_wallPolicy == static_cast<int32_t>(conf::battle::ClipWallPolicy::EndClip) && disp.m_wall)
        return true;
    return false;
}
// 当前片段放完或取消窗口打开后，切下一片段或结束施法。
void finishRoleClip(World& world, Entity entity, ActionTimelineComponent& timeline, SkillCastComponent& cast)
{
    timeline::rules::exitClip(world, entity);
    ++timeline.m_index;
    if (timeline.m_index >= static_cast<int32_t>(timeline.m_clips.size()))
    {
        skill::rules::endCast(world, entity);
        // 施法结束且无缓存下一招：同帧回到待机/跑动（旧实现攻击分支结束即清 Attack 位）。
        if (cast.m_skill == kNoSkill)
            role::rules::exitAttack(world, entity, world.ecs().get<RoleStateComponent>(entity), cast,
                                    world.ecs().get<MotionStateComponent>(entity),
                                    world.ecs().get<IntentComponent>(entity));
        return;
    }
    const int32_t nextId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
    if (!timeline::rules::enterClip(world, entity, nextId, false, false))
        skill::rules::endCast(world, entity);
}
}  // namespace

// 系统 TimelineSystem(roles)：4.3a 行。只有 Attack 推进角色时间轴。
void runTimelineRoles(World& world)
{
    world.ecs().each<ActionTimelineComponent, SkillCastComponent, MotionStateComponent, RoleStateComponent>(
        [&](Entity entity, ActionTimelineComponent& timeline, SkillCastComponent& cast, MotionStateComponent&, const RoleStateComponent& state) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            const auto& born = world.ecs().get<IdentityComponent>(entity).m_born;
            if (born.m_frame == world.frame() && born.m_stage > SpawnStage::RoleTimeline)
                return;
            if (state.m_state != RoleStateId::Attack)
                return;
            if (cast.m_skill == kNoSkill || timeline.m_finished)
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->m_paused)
                    return;
            // 本帧刚进入的片段不推进、不写控制速度，与旧的懒进入同一帧不计时一致。
            if (timeline.m_deferAdvance)
            {
                timeline.m_deferAdvance = false;
                return;
            }
            if (timeline.m_clips.empty() || timeline.m_clipId == 0)
            {
                skill::rules::endCast(world, entity);
                return;
            }
            bool done = timeline::rules::advanceClip(world, entity);
            if (displacementEndsClip(world.ecs().get<DisplacementComponent>(entity)))
                done = true;
            const auto* row = conf::Config::instance().find<conf::battle::ClipDef>(timeline.m_clipId);
            if (!done && row)
            {
                const Fixed interval = "1000"_fx * kLogicDtSec / row->rate;
                const bool normalOpen =
                    row->cancel.normalFrame != -1 &&
                    timeline.m_clipMs > interval * Fixed::fromInt(row->cancel.normalFrame);
                const bool extraOpen =
                    row->cancel.extraFrame != -1 &&
                    timeline.m_clipMs > interval * Fixed::fromInt(row->cancel.extraFrame);
                if (normalOpen)
                {
                    cast.m_allowInterrupt = true;
                    if (cast.m_nextSkill != kNoSkill &&
                        skill::rules::canCancel(world, entity, cast.m_nextSkill, cast.m_skill, false))
                    {
                        done          = true;
                        cast.m_switch = true;
                    }
                    else if (cast.m_nextRun)
                    {
                        done          = true;
                        cast.m_switch = true;
                    }
                }
                if (extraOpen)
                {
                    cast.m_allowExtraInterrupt = true;
                    if (cast.m_nextSkill != kNoSkill &&
                        skill::rules::canCancel(world, entity, cast.m_nextSkill, cast.m_skill, true))
                    {
                        done          = true;
                        cast.m_switch = true;
                    }
                }
            }
            if (done)
                finishRoleClip(world, entity, timeline, cast);
            else
                timeline::rules::applyClipControl(world, entity);
        });
}

// 系统 TimelineSystem(effects)：4.3b 行。片段在出生时已同步进入。
void runTimelineEffects(World& world)
{
    world.ecs().each<ActionTimelineComponent, EffectInfoComponent, MotionStateComponent>(
        [&](Entity entity, ActionTimelineComponent& timeline, const EffectInfoComponent&, MotionStateComponent&) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            const auto& born = world.ecs().get<IdentityComponent>(entity).m_born;
            if (born.m_frame == world.frame() && born.m_stage > SpawnStage::RoleTimeline)
                return;
            if (timeline.m_finished)
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->m_paused)
                    return;
            if (timeline.m_deferAdvance)
            {
                timeline.m_deferAdvance = false;
                return;
            }
            if (timeline.m_clipId == 0)
            {
                timeline.m_finished = true;
                return;
            }
            bool done = timeline::rules::advanceClip(world, entity);
            if (displacementEndsClip(world.ecs().get<DisplacementComponent>(entity)))
                done = true;
            if (!done)
            {
                timeline::rules::applyClipControl(world, entity);
                return;
            }
            timeline::rules::exitClip(world, entity);
            ++timeline.m_index;
            if (timeline.m_index >= static_cast<int32_t>(timeline.m_clips.size()))
            {
                timeline.m_finished = true;
                auto& info = world.ecs().get<EffectInfoComponent>(entity);
                const auto& disp = world.ecs().get<DisplacementComponent>(entity);
                // 撞墙结束片段时禁止后继，对齐旧实现 obstruct。
                if (disp.m_wallPolicy == static_cast<int32_t>(conf::battle::ClipWallPolicy::EndClip) && disp.m_wall)
                    info.m_next = false;
                // 后继特效留给 DespawnSystem（P9）派生，避免子特效在出生帧跑位移。
                life::rules::despawn(world, entity, DespawnReason::ClipEnd);
                return;
            }
            const int32_t nextId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
            if (!timeline::rules::enterClip(world, entity, nextId, true, false))
                timeline.m_finished = true;
        });
    // 命中数达标在上一帧碰撞写入 hitTargets 之后、本帧碰撞之前回收，对齐旧实现特效更新顺序。
    combat::rules::expireByHitCount(world);
}
}  // namespace mugen::sim
