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
    std::variant<int32_t, std::string> motion;
    // 播放倍率。
    Fixed rate;
    // 循环次数，-1 表示无限。
    Fixed loops;
    // 尾延时（毫秒）。
    int32_t tailDelayMs = 0;
    // 位移配置标识，-1 表示不覆盖。
    int32_t displacementId = -1;
    // 进入时准入、退出时移除的片段 Buff。
    std::vector<int32_t> clipBuffs;
    // 按帧升序的 cue。
    std::vector<conf::battle::ClipCue> cues;
    // 落地 / 撞墙 / 刹停结束片段策略。
    conf::battle::ClipFloorPolicy floorPolicy = conf::battle::ClipFloorPolicy::Default;
    conf::battle::ClipWallPolicy wallPolicy   = conf::battle::ClipWallPolicy::Default;
    bool endOnBrake                           = false;
    // 片段控制：FreeMove / SteerAtEnter / TurnOnly / MoveNoTurn。
    conf::battle::ClipControlMode control = conf::battle::ClipControlMode::None;
    // FreeMove / MoveNoTurn 的控制速度。
    Fixed controlVelocity;
};
ClipCommon common(const conf::battle::ClipDef& row)
{
    ClipCommon out;
    out.motion          = row.motion;
    out.rate            = row.rate;
    out.loops           = row.loops;
    out.tailDelayMs     = row.tailDelayMs;
    out.displacementId  = row.displacementId;
    out.clipBuffs       = row.clipBuffs;
    out.cues            = row.cues;
    out.floorPolicy     = row.displacementPolicy.floor;
    out.wallPolicy      = row.displacementPolicy.wall;
    out.endOnBrake      = row.displacementPolicy.endOnBrake != 0;
    out.control         = row.control.mode;
    out.controlVelocity = row.control.velocity;
    return out;
}
ClipCommon common(const conf::battle::EffectClipDef& row)
{
    ClipCommon out;
    out.motion          = row.motion;
    out.rate            = row.rate;
    out.loops           = row.loops;
    out.tailDelayMs     = row.tailDelayMs;
    out.displacementId  = row.displacementId;
    out.cues            = row.cues;
    out.floorPolicy     = row.displacementPolicy.floor;
    out.wallPolicy      = row.displacementPolicy.wall;
    out.endOnBrake      = row.displacementPolicy.endOnBrake != 0;
    out.control         = row.control.mode;
    out.controlVelocity = row.control.velocity;
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
    if (world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead)
        return false;
    if (world.ecs().has<RoleStateComponent>(entity) &&
        world.ecs().get<RoleStateComponent>(entity).state == RoleStateId::Death)
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
        if (cast->skill != kNoSkill)
            source.skillId = book.skills[static_cast<size_t>(cast->skill)].configId;
        source.baseSkillId = source.skillId;
        source.slot        = 0;
    }
    return source;
}
// 特效片段召唤：主人阵营/等级，位置取特效。
void fireSummon(World& world, Entity entity, const conf::battle::ClipCue& cue, ActionTimelineComponent& timeline)
{
    if (timeline.summoned || cue.summonId <= 0)
        return;
    timeline.summoned = true;
    if (!world.ecs().has<EffectInfoComponent>(entity))
        return;
    const Entity owner = world.ecs().get<EffectInfoComponent>(entity).owner;
    if (!roleAlive(world, owner))
        return;
    const auto& ownerId   = world.ecs().get<IdentityComponent>(owner);
    const auto& transform = world.ecs().get<TransformComponent>(entity);
    SpawnRequest request;
    request.kind        = SpawnKind::Role;
    request.configId    = cue.summonId;
    request.owner       = owner;
    request.position    = transform.position;
    request.towardX     = transform.towardX;
    request.vectorX     = transform.vectorX;
    request.vectorZ     = transform.vectorZ;
    request.lifetimeMs  = cue.summonTime > 0 ? static_cast<int32_t>(cue.summonTime) : 0;
    request.level       = ownerId.level;
    request.camp        = ownerId.camp;
    request.isAi        = true;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}
// 角色片段变身：未死且本片段未触发过。
void fireTransform(World& world, Entity entity, const conf::battle::ClipCue& cue, ActionTimelineComponent& timeline)
{
    if (timeline.transformFired || cue.transformId <= 0)
        return;
    timeline.transformFired = true;
    if (world.ecs().has<EffectInfoComponent>(entity) || !roleAlive(world, entity))
        return;
    TransformRequest request;
    request.entity       = entity;
    request.targetRoleId = cue.transformId;
    request.type         = cue.transformType;
    request.actionId     = timeline.clipId;
    world.ecs().singleton<EventQueue<TransformRequest>>().push(request);
}
// 定身：敌方必中；staticTarget≠0 时自己也中。时长 target=2 时用演出帧数。
void fireTimeStop(World& world, Entity entity, const conf::battle::ClipCue& cue, ActionTimelineComponent& timeline)
{
    if (timeline.timeStopOn)
        return;
    const Fixed duration = cue.staticTarget == 2
                               ? Fixed::fromInt(cue.displaySpineFrameCount) * kLogicDtSec * "1000"_fx
                               : Fixed::fromInt(cue.staticTime);
    world.ecs().each<IdentityComponent, PauseComponent>(
        [&](Entity target, const IdentityComponent& identity, PauseComponent& pause) {
            if (world.ecs().has<DespawningComponent>(target) || world.ecs().has<InactiveComponent>(target))
                return;
            if (identity.kind != SpawnKind::Role)
                return;
            const bool self = target == entity && cue.staticTarget != 0;
            const bool enemy = identity.camp == static_cast<int32_t>(EntityCamp::Enemy) ||
                               identity.camp == static_cast<int32_t>(EntityCamp::EnemyFriend);
            if (!self && !enemy)
                return;
            if (duration > pause.staticMs)
                pause.staticMs = duration;
        });
    timeline.timeStopOn        = true;
    timeline.timeStopResetAtMs = world.ecs().singleton<WorldClock>().elapsedMs + Fixed::fromInt(cue.staticResetTime);
}
// 触发到期 cue。SpawnEffect 同帧按 effectId 升序。TimeStop 可在复位后再次触发。
void fireCues(World& world, Entity entity, const ClipCommon& row, ActionTimelineComponent& timeline, Fixed interval)
{
    auto& ecs = world.ecs();
    std::vector<int32_t> due;
    int32_t i = timeline.nextCue;
    for (; i < static_cast<int32_t>(row.cues.size()); ++i)
    {
        const auto& cue = row.cues[static_cast<size_t>(i)];
        if (timeline.clipMs < interval * Fixed::fromInt(cue.frame))
            break;
        due.push_back(i);
    }
    timeline.nextCue = i;
    if (due.size() > 1)
    {
        bool mixed = false;
        for (size_t n = 1; n < due.size(); ++n)
            if (row.cues[static_cast<size_t>(due[n])].frame != row.cues[static_cast<size_t>(due[0])].frame)
                mixed = true;
        if (mixed)
            std::stable_sort(due.begin(), due.end(), [&](int32_t a, int32_t b) {
                const auto& ca = row.cues[static_cast<size_t>(a)];
                const auto& cb = row.cues[static_cast<size_t>(b)];
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
        const auto& cue = row.cues[static_cast<size_t>(index)];
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
        else if (cue.kind == conf::battle::ClipCueKind::Sound && !cue.soundIds.empty())
        {
            CueEvent event;
            event.entity     = entity;
            event.kind       = CueKind::Sound;
            event.param      = timeline.clipId;
            event.candidates = cue.soundIds;
            ecs.singleton<EventQueue<CueEvent>>().push(event);
        }
        else if (cue.kind == conf::battle::ClipCueKind::CameraShake && cue.cameraId > 0)
        {
            CueEvent event;
            event.entity = entity;
            event.kind   = CueKind::ShakeScreen;
            event.id     = cue.cameraId;
            ecs.singleton<EventQueue<CueEvent>>().push(event);
        }
        else if (cue.kind == conf::battle::ClipCueKind::Ghost && cue.value != 0)
        {
            CueEvent event;
            event.entity = entity;
            event.kind   = CueKind::AfterImage;
            event.param  = 1;
            ecs.singleton<EventQueue<CueEvent>>().push(event);
        }
        else if (cue.kind == conf::battle::ClipCueKind::Tips && cue.value > 0)
        {
            CueEvent event;
            event.entity = entity;
            event.kind   = CueKind::Tip;
            event.id     = cue.value;
            ecs.singleton<EventQueue<CueEvent>>().push(event);
        }
        else if (cue.kind == conf::battle::ClipCueKind::Dialog && cue.value > 0)
        {
            CueEvent event;
            event.entity = entity;
            event.kind   = CueKind::Dialogue;
            event.id     = cue.value;
            ecs.singleton<EventQueue<CueEvent>>().push(event);
        }
        else if (cue.kind == conf::battle::ClipCueKind::DisplaySpine && !cue.displaySpineGroups.empty())
        {
            CueEvent event;
            event.entity = entity;
            event.kind   = CueKind::FullScreen;
            event.groups = cue.displaySpineGroups;
            ecs.singleton<EventQueue<CueEvent>>().push(event);
        }
    }
    const Fixed now = ecs.singleton<WorldClock>().elapsedMs;
    if (timeline.timeStopOn && now >= timeline.timeStopResetAtMs)
        timeline.timeStopOn = false;
    if (!timeline.timeStopOn)
    {
        for (const auto& cue : row.cues)
        {
            if (cue.kind != conf::battle::ClipCueKind::TimeStop)
                continue;
            if (timeline.clipMs < interval * Fixed::fromInt(cue.frame))
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
    const auto& move = world.ecs().get<IntentComponent>(stick).move;
    if (move.quadrant == ControlQuadrant::None)
        return;
    auto& transform = world.ecs().get<TransformComponent>(target);
    if (hasHorizontal(move.angle))
    {
        transform.vectorX = Fixed::fromInt(vectorX(move.quadrant));
        if (facing)
            transform.towardX = static_cast<int32_t>(transform.vectorX.toInt());
    }
    if (hasVertical(move.angle))
        transform.vectorZ = Fixed::fromInt(vectorZ(move.quadrant));
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
    motion.motionName     = clipMotionName(row.motion);
    motion.timeMs         = {};
    motion.completedLoops = 0;
    motion.loop           = row.loops == "-1"_fx || row.loops > "1"_fx;
    motion.rate           = row.rate;
    motion.playing        = true;
    timeline.clipId       = clipId;
    timeline.loops        = row.loops;
    timeline.tailMs       = row.tailDelayMs;
    timeline.clipMs       = {};
    timeline.animationEnd = false;
    timeline.nextCue      = 0;
    timeline.spawnedEffects.clear();
    timeline.deferAdvance     = deferAdvance;
    timeline.finished         = false;
    timeline.transformFired   = false;
    timeline.summoned         = false;
    timeline.timeStopOn       = false;
    timeline.timeStopResetAtMs = {};
    BuffContext clipCtx;
    clipCtx.applicator = entity;
    clipCtx.target     = entity;
    if (world.ecs().has<SkillCastComponent>(entity) && world.ecs().has<SkillBookComponent>(entity))
    {
        const auto& cast = world.ecs().get<SkillCastComponent>(entity);
        const auto& book = world.ecs().get<SkillBookComponent>(entity);
        if (cast.skill != kNoSkill)
            clipCtx.boundSkill = book.skills[static_cast<size_t>(cast.skill)].configId;
        clipCtx.skillId = clipCtx.boundSkill;
    }
    for (const int32_t id : row.clipBuffs)
        if (id != -1)
            buff::rules::admit(world, entity, id, clipCtx);
    if (!effect)
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeRoleSkillAction, clipCtx);
    // SteerAtEnter 在设位移前转向，后续曲线按新朝向积分。
    if (!effect && row.control == conf::battle::ClipControlMode::SteerAtEnter)
        applyStickFacing(world, entity, entity, true);
    if (row.displacementId != -1)
        displacement::rules::set(world, entity, row.displacementId);
    else
        displacement::rules::reset(world, entity);
    auto& disp          = world.ecs().get<DisplacementComponent>(entity);
    disp.floorPolicy  = static_cast<int32_t>(row.floorPolicy);
    disp.wallPolicy   = static_cast<int32_t>(row.wallPolicy);
    disp.endOnBrake   = row.endOnBrake;
    disp.brake = disp.air = disp.floor = disp.lie = disp.wall = false;
    ActionEvent began;
    began.entity   = entity;
    began.actionId = clipId;
    began.phase    = ActionPhase::Begin;
    world.ecs().singleton<EventQueue<ActionEvent>>().push(began);
    return true;
}

// 退出当前片段并移除片段 Buff。
void exitClip(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    if (timeline.clipId == 0)
        return;
    ActionEvent ended;
    ended.entity   = entity;
    ended.actionId = timeline.clipId;
    ended.phase    = ActionPhase::End;
    world.ecs().singleton<EventQueue<ActionEvent>>().push(ended);
    const bool effect = world.ecs().has<EffectInfoComponent>(entity);
    ClipCommon row;
    if (loadClip(world, entity, timeline.clipId, effect, row))
    {
        bool stopGhost = false;
        bool stopSound = false;
        for (const auto& cue : row.cues)
        {
            if (cue.kind == conf::battle::ClipCueKind::Ghost && cue.value != 0)
                stopGhost = true;
            if (cue.kind == conf::battle::ClipCueKind::Sound)
                stopSound = true;
        }
        if (stopGhost)
        {
            CueEvent event;
            event.entity = entity;
            event.kind   = CueKind::AfterImage;
            event.param  = 0;
            world.ecs().singleton<EventQueue<CueEvent>>().push(event);
        }
        if (stopSound)
        {
            CueEvent event;
            event.entity   = entity;
            event.kind     = CueKind::Sound;
            event.param    = timeline.clipId;
            event.stopLoop = true;
            world.ecs().singleton<EventQueue<CueEvent>>().push(event);
        }
    }
    if (!effect)
    {
        if (const auto* row = conf::Config::instance().find<conf::battle::ClipDef>(timeline.clipId))
        {
            BuffContext clipCtx;
            clipCtx.target = entity;
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterRoleSkillAction, clipCtx);
            for (const int32_t id : row->clipBuffs)
                if (id != -1)
                    buff::rules::remove(world, entity, id);
        }
        // autoRelease=0：片段退出即回收（对齐旧实现动作结束清特效），后继由 DespawnSystem 派生。
        std::vector<Entity> clipFx;
        world.ecs().each<EffectInfoComponent, LifetimeComponent>(
            [&](Entity effect, const EffectInfoComponent& info, const LifetimeComponent& life) {
                if (info.owner == entity && life.autoRelease == 0 &&
                    !world.ecs().has<DespawningComponent>(effect))
                    clipFx.push_back(effect);
            });
        for (const Entity effect : clipFx)
            life::rules::despawn(world, effect, DespawnReason::ClipEnd);
    }
    timeline.clipId = 0;
    // 片段退出清速度；空中保留曲线等落地，避免 y>0 时提前 disp-。
    displacement::rules::reset(world, entity, false);
    auto& disp         = world.ecs().get<DisplacementComponent>(entity);
    disp.floorPolicy = 0;
    disp.wallPolicy  = 0;
    disp.endOnBrake  = false;
}

// 推进当前片段。
bool advanceClip(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    auto& motion   = world.ecs().get<MotionStateComponent>(entity);
    const bool effect = world.ecs().has<EffectInfoComponent>(entity);
    ClipCommon row;
    if (!loadClip(world, entity, timeline.clipId, effect, row))
        return true;
    const Fixed actionScale = world.ecs().has<StatsComponent>(entity) ? world.ecs().get<StatsComponent>(entity).finalStat(202)
                                                                    : Fixed::fromInt(1);
    const Fixed dtMs     = world.ecs().singleton<WorldClock>().dtMs * actionScale;
    const Fixed interval = clipInterval(row.rate);
    if (!timeline.animationEnd && row.loops != "-1"_fx && Fixed::fromInt(motion.completedLoops) >= row.loops)
    {
        timeline.animationEnd = true;
        timeline.clipMs       = {};
    }
    timeline.clipMs += dtMs;
    fireCues(world, entity, row, timeline, interval);
    return timeline.animationEnd && timeline.clipMs >= Fixed::fromInt(timeline.tailMs);
}

// 按当前片段 control 写朝向与控制速度。SteerAtEnter 只在 enterClip。
void applyClipControl(World& world, Entity entity)
{
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    if (timeline.clipId == 0)
        return;
    const bool effect = world.ecs().has<EffectInfoComponent>(entity);
    ClipCommon row;
    if (!loadClip(world, entity, timeline.clipId, effect, row))
        return;
    const Entity stick = effect ? world.ecs().get<EffectInfoComponent>(entity).owner : entity;
    if (row.control == conf::battle::ClipControlMode::TurnOnly)
    {
        if (row.displacementId != -1)
            applyStickFacing(world, stick, entity, true);
        return;
    }
    if ((row.control == conf::battle::ClipControlMode::FreeMove ||
         row.control == conf::battle::ClipControlMode::MoveNoTurn) &&
        row.displacementId == -1)
    {
        // 旧实现 AttackAction::update 第一次就写控制速度；进入帧不更新由 deferAdvance / Sequence 下一帧再 update 保证。
        applyStickFacing(world, stick, entity, row.control == conf::battle::ClipControlMode::FreeMove);
        auto& velocity = world.ecs().get<DisplacementComponent>(entity).velocity;
        if (!world.ecs().has<IntentComponent>(stick))
        {
            velocity.x = {};
            velocity.z = {};
            return;
        }
        const auto& move  = world.ecs().get<IntentComponent>(stick).move;
        const auto& t     = world.ecs().get<TransformComponent>(entity);
        const Fixed angle = Fixed::ratio(move.angle, 1000);
        velocity.x        = move.quadrant == ControlQuadrant::None
                                ? Fixed{}
                                : FixedMath::cosDeg(angle) * row.controlVelocity * t.vectorX;
        velocity.z        = move.quadrant == ControlQuadrant::None
                                ? Fixed{}
                                : FixedMath::sinDeg(angle) * row.controlVelocity * t.vectorZ;
    }
}
}  // namespace mugen::sim::timeline::rules
