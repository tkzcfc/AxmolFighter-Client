// [4.3a] TimelineSystem(roles) / [4.3b] TimelineSystem(effects)
// 读：`ActionTimeline`、`SkillCast`、`RoleState`、`MotionState` 完成位、`EffectInfo`、`WorldClock`
// 写：`ActionTimeline`、`MotionState`、`Displacement`、`SkillCast`
// 消费：—
// 产生：`SpawnRequest`、`CastEvent`、`BuffEvent`、`DespawnRequest`；特效片段放完打 `Despawning`；位移事实由 displacement::rules 产生
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 当前片段放完或取消窗口打开后，切下一片段或结束施法。
void finishRoleClip(World& world, Entity entity, ActionTimeline& timeline, SkillCast& cast)
{
    timeline::rules::exitClip(world, entity);
    ++timeline.m_index;
    if (timeline.m_index >= static_cast<int32_t>(timeline.m_clips.size()))
    {
        skill::rules::endCast(world, entity);
        return;
    }
    const int32_t nextId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
    if (!timeline::rules::enterClip(world, entity, nextId, false, false))
        skill::rules::endCast(world, entity);
    (void)cast;
}
}  // namespace

// 系统 TimelineSystem(roles)：4.3a 行。只有 Attack 推进角色时间轴。
void runTimelineRoles(World& world)
{
    world.ecs().each<ActionTimeline, SkillCast, MotionState, RoleState>(
        [&](Entity entity, ActionTimeline& timeline, SkillCast& cast, MotionState&, const RoleState& state) {
            if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
                return;
            if (state.m_state != RoleStateId::Attack)
                return;
            if (cast.m_skill == kNoSkill || timeline.m_finished)
                return;
            // 本帧刚进入的片段不推进，与旧的懒进入同一帧不计时一致。
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
            const auto* row = conf::Config::instance().find<conf::battle::ClipDef>(timeline.m_clipId);
            if (!done && row && row->cancel.normalFrame != -1 && cast.m_nextSkill != kNoSkill)
            {
                const Fixed interval = "1000"_fx * kLogicDtSec / row->rate;
                if (timeline.m_clipMs > interval * Fixed::fromInt(row->cancel.normalFrame))
                    done = true;
            }
            if (done)
                finishRoleClip(world, entity, timeline, cast);
        });
}

// 系统 TimelineSystem(effects)：4.3b 行。片段在出生时已同步进入。
void runTimelineEffects(World& world)
{
    world.ecs().each<ActionTimeline, EffectInfo, MotionState>(
        [&](Entity entity, ActionTimeline& timeline, const EffectInfo&, MotionState&) {
            if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
                return;
            if (timeline.m_finished)
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
            if (!timeline::rules::advanceClip(world, entity))
                return;
            timeline::rules::exitClip(world, entity);
            ++timeline.m_index;
            if (timeline.m_index >= static_cast<int32_t>(timeline.m_clips.size()))
            {
                timeline.m_finished = true;
                world.ecs().add(entity, Despawning{});
                world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, DespawnReason::ClipEnd});
                return;
            }
            const int32_t nextId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
            if (!timeline::rules::enterClip(world, entity, nextId, true, false))
                timeline.m_finished = true;
        });
}
}  // namespace mugen::sim
