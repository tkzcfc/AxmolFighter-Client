// [4.3a] TimelineSystem(roles) / [4.3b] TimelineSystem(effects)
// 读：`ActionTimeline`、`SkillCast`、`EffectInfo`、`MotionState`、`WorldClock`
// 写：`ActionTimeline`、`MotionState`、`Displacement`、`SkillCast`
// 消费：—
// 产生：`SpawnRequest`、`CastEvent`、`DispEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 片段动作名（角色/特效共用）。
std::string clipMotionName(const std::variant<int32_t, std::string>& motion)
{
    if (const auto* name = std::get_if<std::string>(&motion))
        return *name;
    return std::to_string(std::get<int32_t>(motion));
}
// 逻辑帧间隔（毫秒）。
Fixed clipInterval(Fixed rate)
{
    return "1000"_fx * kLogicDtSec / rate;
}
// 片段公共字段（角色 ClipDef 与特效 EffectClipDef 共用）。
struct ClipCommon
{
    std::variant<int32_t, std::string> motion;
    Fixed rate;
    Fixed loops;
    int32_t tailDelayMs = 0;
    int32_t displacementId = -1;
    std::vector<int32_t> clipBuffs;
    std::vector<conf::battle::ClipCue> cues;
};
ClipCommon common(const conf::battle::ClipDef& row)
{
    return {row.motion, row.rate, row.loops, row.tailDelayMs, row.displacementId, row.clipBuffs, row.cues};
}
ClipCommon common(const conf::battle::EffectClipDef& row)
{
    return {row.motion, row.rate, row.loops, row.tailDelayMs, row.displacementId, {}, row.cues};
}
// 退出当前片段的片段 Buff（片段退出即移除）。
void exitClipBuffs(World& world, Entity entity, const std::vector<int32_t>& clipBuffs)
{
    for (const int32_t id : clipBuffs)
        if (id != -1)
            buff::rules::remove(world, entity, id);
}
// 进入当前片段。
void enterClip(World& world, Entity entity, const ClipCommon& row, ActionTimeline& timeline, MotionState& motion)
{
    motion.m_motionName     = clipMotionName(row.motion);
    motion.m_timeMs         = {};
    motion.m_completedLoops = 0;
    motion.m_loop           = row.loops == "-1"_fx || row.loops > "1"_fx;
    motion.m_rate           = row.rate;
    motion.m_playing        = true;
    timeline.m_loops        = row.loops;
    timeline.m_tailMs       = row.tailDelayMs;
    timeline.m_clipMs       = {};
    timeline.m_animationEnd = false;
    timeline.m_nextCue      = 0;
    timeline.m_spawnedEffects.clear();
    // 片段进入时准入片段 Buff（退出时移除）。
    for (const int32_t id : row.clipBuffs)
        if (id != -1)
            buff::rules::admit(world, entity, id);
    // 位移覆盖的 disp- 由 displacement::rules::set 处理（set 覆盖全部位移字段）。
    if (row.displacementId != -1)
    {
        if (displacement::rules::set(world, entity, row.displacementId))
        {
            DispEvent event;
            event.m_entity         = entity;
            event.m_displacementId = row.displacementId;
            event.m_phase          = 0;
            world.ecs().singleton<EventQueue<DispEvent>>().push(event);
        }
    }
    else
        displacement::rules::reset(world, entity);
}
// 触发当前片段到达的 SpawnEffect cue。
void fireCues(World& world, Entity entity, const ClipCommon& row, ActionTimeline& timeline, Fixed interval)
{
    auto& ecs = world.ecs();
    for (int32_t i = timeline.m_nextCue; i < static_cast<int32_t>(row.cues.size()); ++i)
    {
        const auto& cue = row.cues[static_cast<size_t>(i)];
        if (timeline.m_clipMs < interval * Fixed::fromInt(cue.frame))
            break;
        timeline.m_nextCue = i + 1;
        if (cue.kind == conf::battle::ClipCueKind::SpawnEffect && cue.effectId != 0)
        {
            SourceContext source;
            if (const auto* cast = ecs.tryGet<SkillCast>(entity))
            {
                const auto& book = ecs.get<SkillBook>(entity);
                if (cast->m_skill != kNoSkill)
                    source.m_skillId = book.m_skills[static_cast<size_t>(cast->m_skill)].m_configId;
                source.m_baseSkillId = source.m_skillId;
                source.m_slot        = 0;
            }
            effect::rules::spawnFromRole(world, entity, cue.effectId, source);
        }
    }
}
// 推进当前片段；返回是否片段完成。
bool advanceClip(World& world, Entity entity, const ClipCommon& row, ActionTimeline& timeline, MotionState& motion)
{
    auto& ecs       = world.ecs();
    const Fixed dtMs = ecs.singleton<WorldClock>().m_dtMs;
    const Fixed interval = clipInterval(row.rate);
    if (!timeline.m_animationEnd && row.loops != "-1"_fx && Fixed::fromInt(motion.m_completedLoops) >= row.loops)
    {
        timeline.m_animationEnd = true;
        timeline.m_clipMs       = {};  // 动画完成时重置片段时间，尾延时从动画完成起算
    }
    timeline.m_clipMs += dtMs;
    fireCues(world, entity, row, timeline, interval);
    return timeline.m_animationEnd && timeline.m_clipMs >= Fixed::fromInt(timeline.m_tailMs);
}
}  // namespace

// 系统 TimelineSystem(roles)：4.3a 行，见 DESIGN.md §3.2。
void runTimelineRoles(World& world)
{
    world.ecs().each<ActionTimeline, SkillCast, MotionState>([&](Entity entity, ActionTimeline& timeline,
                                                                 SkillCast& cast, MotionState& motion) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        if (cast.m_skill == kNoSkill || timeline.m_finished)
            return;
        if (timeline.m_index == 0 && timeline.m_clipId == 0)
        {
            if (timeline.m_clips.empty())
            {
                skill::rules::endCast(world, entity);
                return;
            }
            const int32_t id = timeline.m_clips[0];
            const auto* row  = conf::Config::instance().find<conf::battle::ClipDef>(id);
            if (!row)
            {
                skill::rules::endCast(world, entity);
                return;
            }
            timeline.m_clipId = id;
            enterClip(world, entity, common(*row), timeline, motion);
            return;
        }
        const int32_t clipId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
        const auto* row      = conf::Config::instance().find<conf::battle::ClipDef>(clipId);
        if (!row)
        {
            skill::rules::endCast(world, entity);
            return;
        }
        bool done = advanceClip(world, entity, common(*row), timeline, motion);
        // 普通取消窗口：片段时间越过窗口且已有缓存下一招时结束当前片段。
        if (!done && row->cancel.normalFrame != -1 &&
            timeline.m_clipMs > clipInterval(row->rate) * Fixed::fromInt(row->cancel.normalFrame) &&
            cast.m_nextSkill != kNoSkill)
            done = true;
        if (done)
        {
            exitClipBuffs(world, entity, row->clipBuffs);
            ++timeline.m_index;
            if (timeline.m_index >= static_cast<int32_t>(timeline.m_clips.size()))
            {
                skill::rules::endCast(world, entity);
                // endCast 可能已接续下一招（beginCast 重置时间轴）；同帧进入下一招首片段。
                if (cast.m_skill != kNoSkill && !timeline.m_finished && timeline.m_index == 0 &&
                    timeline.m_clipId == 0 && !timeline.m_clips.empty())
                {
                    const auto* first = conf::Config::instance().find<conf::battle::ClipDef>(timeline.m_clips[0]);
                    if (first)
                    {
                        timeline.m_clipId = timeline.m_clips[0];
                        enterClip(world, entity, common(*first), timeline, motion);
                    }
                }
                return;
            }
            const int32_t nextId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
            const auto* nextRow  = conf::Config::instance().find<conf::battle::ClipDef>(nextId);
            if (!nextRow)
            {
                skill::rules::endCast(world, entity);
                return;
            }
            timeline.m_clipId = nextId;
            enterClip(world, entity, common(*nextRow), timeline, motion);
        }
    });
}

// 系统 TimelineSystem(effects)：4.3b 行，见 DESIGN.md §3.2。
void runTimelineEffects(World& world)
{
    world.ecs().each<ActionTimeline, EffectInfo, MotionState>([&](Entity entity, ActionTimeline& timeline,
                                                                  const EffectInfo&, MotionState& motion) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        if (timeline.m_finished)
            return;
        if (timeline.m_index == 0 && timeline.m_clipId == 0)
        {
            if (timeline.m_clips.empty())
            {
                timeline.m_finished = true;
                return;
            }
            const int32_t id = timeline.m_clips[0];
            const auto* row  = conf::Config::instance().find<conf::battle::EffectClipDef>(id);
            if (!row)
            {
                timeline.m_finished = true;
                return;
            }
            timeline.m_clipId = id;
            enterClip(world, entity, common(*row), timeline, motion);
            return;
        }
        const int32_t clipId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
        const auto* row      = conf::Config::instance().find<conf::battle::EffectClipDef>(clipId);
        if (!row)
        {
            timeline.m_finished = true;
            return;
        }
        if (advanceClip(world, entity, common(*row), timeline, motion))
        {
            ++timeline.m_index;
            if (timeline.m_index >= static_cast<int32_t>(timeline.m_clips.size()))
            {
                timeline.m_finished = true;
                world.ecs().add(entity, Despawning{});
                world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, DespawnReason::ClipEnd});
                return;
            }
            const int32_t nextId = timeline.m_clips[static_cast<size_t>(timeline.m_index)];
            const auto* nextRow  = conf::Config::instance().find<conf::battle::EffectClipDef>(nextId);
            if (!nextRow)
            {
                timeline.m_finished = true;
                return;
            }
            timeline.m_clipId = nextId;
            enterClip(world, entity, common(*nextRow), timeline, motion);
        }
    });
}
}  // namespace mugen::sim
