#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/battle/ClipDef.h"
#include "mugen/core/math/FixedMath.h"
#include <algorithm>
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
    // 落地 / 撞墙 / 刹停结束片段策略。
    conf::battle::ClipFloorPolicy m_floorPolicy = conf::battle::ClipFloorPolicy::Default;
    conf::battle::ClipWallPolicy m_wallPolicy   = conf::battle::ClipWallPolicy::Default;
    bool m_endOnBrake                           = false;
    // 片段控制：FreeMove / SteerAtEnter / TurnOnly / MoveNoTurn。
    conf::battle::ClipControlMode m_control = conf::battle::ClipControlMode::None;
    // FreeMove / MoveNoTurn 的控制速度。
    Fixed m_controlVelocity;
};
ClipCommon common(const conf::battle::ClipDef& row)
{
    ClipCommon out;
    out.m_motion          = row.motion;
    out.m_rate            = row.rate;
    out.m_loops           = row.loops;
    out.m_tailDelayMs     = row.tailDelayMs;
    out.m_displacementId  = row.displacementId;
    out.m_clipBuffs       = row.clipBuffs;
    out.m_cues            = row.cues;
    out.m_floorPolicy     = row.displacementPolicy.floor;
    out.m_wallPolicy      = row.displacementPolicy.wall;
    out.m_endOnBrake      = row.displacementPolicy.endOnBrake != 0;
    out.m_control         = row.control.mode;
    out.m_controlVelocity = row.control.velocity;
    return out;
}
ClipCommon common(const conf::battle::EffectClipDef& row)
{
    ClipCommon out;
    out.m_motion          = row.motion;
    out.m_rate            = row.rate;
    out.m_loops           = row.loops;
    out.m_tailDelayMs     = row.tailDelayMs;
    out.m_displacementId  = row.displacementId;
    out.m_cues            = row.cues;
    out.m_floorPolicy     = row.displacementPolicy.floor;
    out.m_wallPolicy      = row.displacementPolicy.wall;
    out.m_endOnBrake      = row.displacementPolicy.endOnBrake != 0;
    out.m_control         = row.control.mode;
    out.m_controlVelocity = row.control.velocity;
    return out;
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
// 角色未死。
bool roleAlive(World& world, Entity entity)
{
    if (!world.ecs().alive(entity))
        return false;
    if (world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).m_dead)
        return false;
    if (world.ecs().has<RoleStateComponent>(entity) &&
        world.ecs().get<RoleStateComponent>(entity).m_state == RoleStateId::Death)
        return false;
    return true;
}
// 角色时间轴 SpawnEffect 的技能来源。
SourceContext sourceFromCast(World& world, Entity entity)
{
    SourceContext source;
    if (const auto* cast = world.ecs().tryGet<SkillCastComponent>(entity))
    {
        const auto& book = world.ecs().get<SkillBookComponent>(entity);
        if (cast->m_skill != kNoSkill)
            source.m_skillId = book.m_skills[static_cast<size_t>(cast->m_skill)].m_configId;
        source.m_baseSkillId = source.m_skillId;
        source.m_slot        = 0;
    }
    return source;
}
// 特效片段召唤：主人阵营/等级，位置取特效。
void fireSummon(World& world, Entity entity, const conf::battle::ClipCue& cue, ActionTimelineComponent& timeline)
{
    if (timeline.m_summoned || cue.summonId <= 0)
        return;
    timeline.m_summoned = true;
    if (!world.ecs().has<EffectInfoComponent>(entity))
        return;
    const Entity owner = world.ecs().get<EffectInfoComponent>(entity).m_owner;
    if (!roleAlive(world, owner))
        return;
    const auto& ownerId   = world.ecs().get<IdentityComponent>(owner);
    const auto& transform = world.ecs().get<TransformComponent>(entity);
    SpawnRequest request;
    request.m_kind        = SpawnKind::Role;
    request.m_configId    = cue.summonId;
    request.m_owner       = owner;
    request.m_position    = transform.m_position;
    request.m_towardX     = transform.m_towardX;
    request.m_vectorX     = transform.m_vectorX;
    request.m_vectorZ     = transform.m_vectorZ;
    request.m_lifetimeMs  = cue.summonTime > 0 ? static_cast<int32_t>(cue.summonTime) : 0;
    request.m_level       = ownerId.m_level;
    request.m_camp        = ownerId.m_camp;
    request.m_isAi        = true;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}
// 角色片段变身：未死且本片段未触发过。
void fireTransform(World& world, Entity entity, const conf::battle::ClipCue& cue, ActionTimelineComponent& timeline)
{
    if (timeline.m_transformFired || cue.transformId <= 0)
        return;
    timeline.m_transformFired = true;
    if (world.ecs().has<EffectInfoComponent>(entity) || !roleAlive(world, entity))
        return;
    TransformRequest request;
    request.m_entity       = entity;
    request.m_targetRoleId = cue.transformId;
    request.m_type         = cue.transformType;
    request.m_actionId     = timeline.m_clipId;
    world.ecs().singleton<EventQueue<TransformRequest>>().push(request);
}
// 定身：敌方必中；staticTarget≠0 时自己也中。时长 target=2 时用演出帧数。
void fireTimeStop(World& world, Entity entity, const conf::battle::ClipCue& cue, ActionTimelineComponent& timeline)
{
    if (timeline.m_timeStopOn)
        return;
    const Fixed duration = cue.staticTarget == 2
                               ? Fixed::fromInt(cue.displaySpineFrameCount) * kLogicDtSec * "1000"_fx
                               : Fixed::fromInt(cue.staticTime);
    world.ecs().each<IdentityComponent, PauseComponent>(
        [&](Entity target, const IdentityComponent& identity, PauseComponent& pause) {
            if (world.ecs().has<DespawningComponent>(target) || world.ecs().has<InactiveComponent>(target))
                return;
            if (identity.m_kind != SpawnKind::Role)
                return;
            const bool self = target == entity && cue.staticTarget != 0;
            const bool enemy = identity.m_camp == static_cast<int32_t>(EntityCamp::Enemy) ||
                               identity.m_camp == static_cast<int32_t>(EntityCamp::EnemyFriend);
            if (!self && !enemy)
                return;
            if (duration > pause.m_staticMs)
                pause.m_staticMs = duration;
        });
    timeline.m_timeStopOn        = true;
    timeline.m_timeStopResetAtMs = world.ecs().singleton<WorldClock>().m_elapsedMs + Fixed::fromInt(cue.staticResetTime);
}
// 触发到期 cue。SpawnEffect 同帧按 effectId 升序。TimeStop 可在复位后再次触发。
void fireCues(World& world, Entity entity, const ClipCommon& row, ActionTimelineComponent& timeline, Fixed interval)
{
    auto& ecs = world.ecs();
    std::vector<int32_t> due;
    int32_t i = timeline.m_nextCue;
    for (; i < static_cast<int32_t>(row.m_cues.size()); ++i)
    {
        const auto& cue = row.m_cues[static_cast<size_t>(i)];
        if (timeline.m_clipMs < interval * Fixed::fromInt(cue.frame))
            break;
        due.push_back(i);
    }
    timeline.m_nextCue = i;
    if (due.size() > 1)
    {
        bool mixed = false;
        for (size_t n = 1; n < due.size(); ++n)
            if (row.m_cues[static_cast<size_t>(due[n])].frame != row.m_cues[static_cast<size_t>(due[0])].frame)
                mixed = true;
        if (mixed)
            std::stable_sort(due.begin(), due.end(), [&](int32_t a, int32_t b) {
                const auto& ca = row.m_cues[static_cast<size_t>(a)];
                const auto& cb = row.m_cues[static_cast<size_t>(b)];
                const bool spawnA = ca.kind == conf::battle::ClipCueKind::SpawnEffect;
                const bool spawnB = cb.kind == conf::battle::ClipCueKind::SpawnEffect;
                if (spawnA != spawnB)
                    return spawnA;
                if (spawnA && ca.effectId != cb.effectId)
                    return ca.effectId < cb.effectId;
                return a < b;
            });
    }
    for (const int32_t index : due)
    {
        const auto& cue = row.m_cues[static_cast<size_t>(index)];
        if (cue.kind == conf::battle::ClipCueKind::SpawnEffect && cue.effectId != 0)
        {
            // 特效片段子特效挂在主人键上（spawnChild）；角色片段挂在自己身上。
            if (ecs.has<EffectInfoComponent>(entity))
                effect::rules::spawnChild(world, entity, cue.effectId);
            else
                effect::rules::spawnFromRole(world, entity, cue.effectId, sourceFromCast(world, entity));
        }
        else if (cue.kind == conf::battle::ClipCueKind::Summon)
            fireSummon(world, entity, cue, timeline);
        else if (cue.kind == conf::battle::ClipCueKind::Transform)
            fireTransform(world, entity, cue, timeline);
    }
    const Fixed now = ecs.singleton<WorldClock>().m_elapsedMs;
    if (timeline.m_timeStopOn && now >= timeline.m_timeStopResetAtMs)
        timeline.m_timeStopOn = false;
    if (!timeline.m_timeStopOn)
    {
        for (const auto& cue : row.m_cues)
        {
            if (cue.kind != conf::battle::ClipCueKind::TimeStop)
                continue;
            if (timeline.m_clipMs < interval * Fixed::fromInt(cue.frame))
                break;
            fireTimeStop(world, entity, cue, timeline);
            break;
        }
    }
}
// 把摇杆方向写到目标变换。facing 为 true 时同步 towardX。
void applyStickFacing(World& world, Entity stick, Entity target, bool facing)
{
    if (!world.ecs().has<IntentComponent>(stick))
        return;
    const auto& move = world.ecs().get<IntentComponent>(stick).m_move;
    if (move.m_quadrant == ControlQuadrant::None)
        return;
    auto& transform = world.ecs().get<TransformComponent>(target);
    if (hasHorizontal(move.m_angle))
    {
        transform.m_vectorX = Fixed::fromInt(vectorX(move.m_quadrant));
        if (facing)
            transform.m_towardX = static_cast<int32_t>(transform.m_vectorX.toInt());
    }
    if (hasVertical(move.m_angle))
        transform.m_vectorZ = Fixed::fromInt(vectorZ(move.m_quadrant));
}
}  // namespace

// 进入指定片段。
bool enterClip(World& world, Entity entity, int32_t clipId, bool effect, bool deferAdvance)
{
    ClipCommon row;
    if (!loadClip(world, entity, clipId, effect, row))
        return false;
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    auto& motion   = world.ecs().get<MotionStateComponent>(entity);
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
    timeline.m_deferAdvance     = deferAdvance;
    timeline.m_finished         = false;
    timeline.m_transformFired   = false;
    timeline.m_summoned         = false;
    timeline.m_timeStopOn       = false;
    timeline.m_timeStopResetAtMs = {};
    BuffContext clipCtx;
    clipCtx.m_applicator = entity;
    clipCtx.m_target     = entity;
    if (world.ecs().has<SkillCastComponent>(entity) && world.ecs().has<SkillBookComponent>(entity))
    {
        const auto& cast = world.ecs().get<SkillCastComponent>(entity);
        const auto& book = world.ecs().get<SkillBookComponent>(entity);
        if (cast.m_skill != kNoSkill)
            clipCtx.m_boundSkill = book.m_skills[static_cast<size_t>(cast.m_skill)].m_configId;
        clipCtx.m_skillId = clipCtx.m_boundSkill;
    }
    for (const int32_t id : row.m_clipBuffs)
        if (id != -1)
            buff::rules::admit(world, entity, id, clipCtx);
    if (!effect)
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeRoleSkillAction, clipCtx);
    // SteerAtEnter 在设位移前转向，后续曲线按新朝向积分。
    if (!effect && row.m_control == conf::battle::ClipControlMode::SteerAtEnter)
        applyStickFacing(world, entity, entity, true);
    if (row.m_displacementId != -1)
        displacement::rules::set(world, entity, row.m_displacementId);
    else
        displacement::rules::reset(world, entity);
    auto& disp          = world.ecs().get<DisplacementComponent>(entity);
    disp.m_floorPolicy  = static_cast<int32_t>(row.m_floorPolicy);
    disp.m_wallPolicy   = static_cast<int32_t>(row.m_wallPolicy);
    disp.m_endOnBrake   = row.m_endOnBrake;
    disp.m_brake = disp.m_air = disp.m_floor = disp.m_lie = disp.m_wall = false;
    return true;
}

// 退出当前片段并移除片段 Buff。
void exitClip(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    if (timeline.m_clipId == 0)
        return;
    if (!world.ecs().has<EffectInfoComponent>(entity))
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
        // autoRelease=0：片段退出即回收（对齐旧实现动作结束清特效），后继由 DespawnSystem 派生。
        std::vector<Entity> clipFx;
        world.ecs().each<EffectInfoComponent, LifetimeComponent>(
            [&](Entity effect, const EffectInfoComponent& info, const LifetimeComponent& life) {
                if (info.m_owner == entity && life.m_autoRelease == 0 &&
                    !world.ecs().has<DespawningComponent>(effect))
                    clipFx.push_back(effect);
            });
        for (const Entity effect : clipFx)
            life::rules::despawn(world, effect, DespawnReason::ClipEnd);
    }
    timeline.m_clipId = 0;
    // 片段退出清速度；空中保留曲线等落地，避免 y>0 时提前 disp-。
    displacement::rules::reset(world, entity, false);
    auto& disp         = world.ecs().get<DisplacementComponent>(entity);
    disp.m_floorPolicy = 0;
    disp.m_wallPolicy  = 0;
    disp.m_endOnBrake  = false;
}

// 推进当前片段。
bool advanceClip(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    auto& motion   = world.ecs().get<MotionStateComponent>(entity);
    const bool effect = world.ecs().has<EffectInfoComponent>(entity);
    ClipCommon row;
    if (!loadClip(world, entity, timeline.m_clipId, effect, row))
        return true;
    const Fixed actionScale = world.ecs().has<StatsComponent>(entity) ? world.ecs().get<StatsComponent>(entity).finalStat(202)
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

// 按当前片段 control 写朝向与控制速度。SteerAtEnter 只在 enterClip。
void applyClipControl(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    if (timeline.m_clipId == 0)
        return;
    const bool effect = world.ecs().has<EffectInfoComponent>(entity);
    ClipCommon row;
    if (!loadClip(world, entity, timeline.m_clipId, effect, row))
        return;
    const Entity stick = effect ? world.ecs().get<EffectInfoComponent>(entity).m_owner : entity;
    if (row.m_control == conf::battle::ClipControlMode::TurnOnly)
    {
        if (row.m_displacementId != -1)
            applyStickFacing(world, stick, entity, true);
        return;
    }
    if ((row.m_control == conf::battle::ClipControlMode::FreeMove ||
         row.m_control == conf::battle::ClipControlMode::MoveNoTurn) &&
        row.m_displacementId == -1)
    {
        // 旧实现 AttackAction::update 第一次就写控制速度；进入帧不更新由 deferAdvance / Sequence 下一帧再 update 保证。
        applyStickFacing(world, stick, entity, row.m_control == conf::battle::ClipControlMode::FreeMove);
        auto& velocity = world.ecs().get<DisplacementComponent>(entity).m_velocity;
        if (!world.ecs().has<IntentComponent>(stick))
        {
            velocity.x = {};
            velocity.z = {};
            return;
        }
        const auto& move  = world.ecs().get<IntentComponent>(stick).m_move;
        const auto& t     = world.ecs().get<TransformComponent>(entity);
        const Fixed angle = Fixed::ratio(move.m_angle, 1000);
        velocity.x        = move.m_quadrant == ControlQuadrant::None
                                ? Fixed{}
                                : FixedMath::cosDeg(angle) * row.m_controlVelocity * t.m_vectorX;
        velocity.z        = move.m_quadrant == ControlQuadrant::None
                                ? Fixed{}
                                : FixedMath::sinDeg(angle) * row.m_controlVelocity * t.m_vectorZ;
    }
}
}  // namespace mugen::sim::timeline::rules
