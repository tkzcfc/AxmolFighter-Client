#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include <string>
#include <variant>
#include <vector>

namespace mugen::sim::timeline::rules
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
    // 动作名或动作索引。
    std::variant<int32_t, std::string> m_motion;
    // 播放倍率。
    Fixed m_rate;
    // 循环次数，-1 表示无限。
    Fixed m_loops;
    // 尾延时（毫秒）。
    int32_t m_tailDelayMs = 0;
    // 位移配置标识，-1 表示不覆盖。
    int32_t m_displacementId = -1;
    // 进入时准入、退出时移除的片段 Buff。
    std::vector<int32_t> m_clipBuffs;
    // 按帧升序的 cue。
    std::vector<conf::battle::ClipCue> m_cues;
};
ClipCommon common(const conf::battle::ClipDef& row)
{
    return {row.motion, row.rate, row.loops, row.tailDelayMs, row.displacementId, row.clipBuffs, row.cues};
}
ClipCommon common(const conf::battle::EffectClipDef& row)
{
    return {row.motion, row.rate, row.loops, row.tailDelayMs, row.displacementId, {}, row.cues};
}
// 按实体种类读取当前片段。配置缺失返回 false。
bool loadClip(World&, Entity, int32_t clipId, bool effect, ClipCommon& out)
{
    if (effect)
    {
        const auto* row = conf::Config::instance().find<conf::battle::EffectClipDef>(clipId);
        if (!row)
            return false;
        out = common(*row);
        return true;
    }
    const auto* row = conf::Config::instance().find<conf::battle::ClipDef>(clipId);
    if (!row)
        return false;
    out = common(*row);
    return true;
}
// 触发当前片段到达的 SpawnEffect cue。
void fireCues(World& world, Entity entity, const ClipCommon& row, ActionTimeline& timeline, Fixed interval)
{
    auto& ecs = world.ecs();
    for (int32_t i = timeline.m_nextCue; i < static_cast<int32_t>(row.m_cues.size()); ++i)
    {
        const auto& cue = row.m_cues[static_cast<size_t>(i)];
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
}  // namespace

// 进入指定片段。
bool enterClip(World& world, Entity entity, int32_t clipId, bool effect, bool deferAdvance)
{
    ClipCommon row;
    if (!loadClip(world, entity, clipId, effect, row))
        return false;
    auto& timeline = world.ecs().get<ActionTimeline>(entity);
    auto& motion   = world.ecs().get<MotionState>(entity);
    motion.m_motionName     = clipMotionName(row.m_motion);
    motion.m_timeMs         = {};
    motion.m_completedLoops = 0;
    motion.m_loop           = row.m_loops == "-1"_fx || row.m_loops > "1"_fx;
    motion.m_rate           = row.m_rate;
    motion.m_playing        = true;
    timeline.m_clipId       = clipId;
    timeline.m_loops        = row.m_loops;
    timeline.m_tailMs       = row.m_tailDelayMs;
    timeline.m_clipMs       = {};
    timeline.m_animationEnd = false;
    timeline.m_nextCue      = 0;
    timeline.m_spawnedEffects.clear();
    timeline.m_deferAdvance = deferAdvance;
    timeline.m_finished     = false;
    BuffContext clipCtx;
    clipCtx.m_applicator = entity;
    clipCtx.m_target     = entity;
    if (world.ecs().has<SkillCast>(entity) && world.ecs().has<SkillBook>(entity))
    {
        const auto& cast = world.ecs().get<SkillCast>(entity);
        const auto& book = world.ecs().get<SkillBook>(entity);
        if (cast.m_skill != kNoSkill)
            clipCtx.m_boundSkill = book.m_skills[static_cast<size_t>(cast.m_skill)].m_configId;
        clipCtx.m_skillId = clipCtx.m_boundSkill;
    }
    for (const int32_t id : row.m_clipBuffs)
        if (id != -1)
            buff::rules::admit(world, entity, id, clipCtx);
    if (!effect)
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeRoleSkillAction, clipCtx);
    if (row.m_displacementId != -1)
        displacement::rules::set(world, entity, row.m_displacementId);
    else
        displacement::rules::reset(world, entity);
    return true;
}

// 退出当前片段并移除片段 Buff。
void exitClip(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimeline>(entity);
    if (timeline.m_clipId == 0)
        return;
    if (!world.ecs().has<EffectInfo>(entity))
    {
        if (const auto* row = conf::Config::instance().find<conf::battle::ClipDef>(timeline.m_clipId))
        {
            BuffContext clipCtx;
            clipCtx.m_target = entity;
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterRoleSkillAction, clipCtx);
            for (const int32_t id : row->clipBuffs)
                if (id != -1)
                    buff::rules::remove(world, entity, id);
        }
    }
    timeline.m_clipId = 0;
}

// 推进当前片段。
bool advanceClip(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimeline>(entity);
    auto& motion   = world.ecs().get<MotionState>(entity);
    const bool effect = world.ecs().has<EffectInfo>(entity);
    ClipCommon row;
    if (!loadClip(world, entity, timeline.m_clipId, effect, row))
        return true;
    const Fixed actionScale = world.ecs().has<Stats>(entity) ? world.ecs().get<Stats>(entity).finalStat(202)
                                                                    : Fixed::fromInt(1);
    const Fixed dtMs     = world.ecs().singleton<WorldClock>().m_dtMs * actionScale;
    const Fixed interval = clipInterval(row.m_rate);
    if (!timeline.m_animationEnd && row.m_loops != "-1"_fx && Fixed::fromInt(motion.m_completedLoops) >= row.m_loops)
    {
        timeline.m_animationEnd = true;
        timeline.m_clipMs       = {};
    }
    timeline.m_clipMs += dtMs;
    fireCues(world, entity, row, timeline, interval);
    return timeline.m_animationEnd && timeline.m_clipMs >= Fixed::fromInt(timeline.m_tailMs);
}
}  // namespace mugen::sim::timeline::rules
