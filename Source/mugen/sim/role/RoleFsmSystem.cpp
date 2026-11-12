// [4.2] RoleFsmSystem
// 读：`RoleStateComponent`（含木偶请求）、`SkillCastComponent`、`IntentComponent.m_move`、`HitStateComponent`、`HealthComponent`、`DisplacementComponent` 事件位、`MotionStateComponent` 完成位
// 写：`RoleStateComponent`（工厂初始化之外的唯一写入者）；进入/退出动作写 `MotionStateComponent`、`DisplacementComponent`、`SkillCastComponent`、`ActionTimelineComponent`（经 skill::rules）
// 消费：—
// 产生：`StateEvent`、`CastEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 唯一状态写入：更新状态、清零计时并发出带正确 m_from 的 StateEvent。
void transition(World& world, Entity entity, RoleStateComponent& state, RoleStateId to)
{
    if (state.m_state == to)
        return;
    const RoleStateId from = state.m_state;
    BuffContext ended;
    ended.m_target = entity;
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::BehaviorStateEnd, ended);
    state.m_state   = to;
    state.m_stateMs = {};
    BuffContext began;
    began.m_target = entity;
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::BehaviorStateStart, began);
    StateEvent event;
    event.m_entity = entity;
    event.m_from   = static_cast<int32_t>(from);
    event.m_to     = static_cast<int32_t>(to);
    world.ecs().singleton<EventQueue<StateEvent>>().push(event);
}
// 播放受击动作。
void playHitMotion(MotionStateComponent& motion)
{
    motion.m_motionName     = role::motionName(RoleStateId::Hit);
    motion.m_timeMs         = {};
    motion.m_completedLoops = 0;
    motion.m_loop           = false;
    motion.m_rate           = conf::kHitScaleTime;
    motion.m_playing        = true;
}
// 启动受击位移。Began 由 displacement::rules::set 发出。
void setHitDisplacement(World& world, Entity entity, HitStateComponent& hit)
{
    if (hit.m_displacementId != -1)
        displacement::rules::set(world, entity, hit.m_displacementId);
}
// 进入受击：首次进入与跨周期重进都复位硬直计时、完成位与状态计时。
void enterHit(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion)
{
    hit.m_hitDefId             = hit.m_pendingHitDefId;
    hit.m_displacementId       = hit.m_pendingDisplacementId;
    hit.m_towardX              = hit.m_pendingTowardX;
    hit.m_pendingHitDefId      = 0;
    hit.m_animationEnd         = false;
    hit.m_displacementEnd      = false;
    hit.m_hitTimeMs            = {};
    playHitMotion(motion);
    setHitDisplacement(world, entity, hit);
    state.m_stateMs = {};
}
// 受击追击重播：保留动画完成位与硬直计时，只复位位移完成位。
void replayHit(World& world, Entity entity, HitStateComponent& hit, MotionStateComponent& motion)
{
    hit.m_hitDefId        = hit.m_pendingHitDefId;
    hit.m_displacementId  = hit.m_pendingDisplacementId;
    hit.m_towardX         = hit.m_pendingTowardX;
    hit.m_pendingHitDefId = 0;
    hit.m_displacementEnd = false;
    hit.m_animationEnd    = false;
    playHitMotion(motion);
    setHitDisplacement(world, entity, hit);
}
// 播放待机动作。
void playIdle(MotionStateComponent& motion)
{
    motion.m_motionName     = role::motionName(RoleStateId::Idle);
    motion.m_timeMs         = {};
    motion.m_completedLoops = 0;
    motion.m_loop           = true;
    motion.m_rate           = conf::kIdleScaleTime;
    motion.m_playing        = true;
}
// 进入死亡：清施法、播放死亡动作并启动死亡位移。
void enterDeath(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                DisplacementComponent& disp, const IdentityComponent& identity)
{
    skill::rules::forceInterrupt(world, entity);
    motion.m_motionName     = role::motionName(RoleStateId::Death);
    motion.m_timeMs         = {};
    motion.m_loop           = false;
    motion.m_completedLoops = 0;
    motion.m_playing        = true;
    hit.m_animationEnd      = false;
    hit.m_displacementEnd   = false;
    hit.m_pendingHitDefId   = 0;
    disp.m_brake            = false;
    disp.m_lie              = false;
    state.m_stateMs         = {};
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
    if (role && role->deathDisplacementId > 0)
        displacement::rules::set(world, entity, role->deathDisplacementId);
}
// 死亡体维持：读上一帧动画与位移完成位。
void updateDeath(const MotionStateComponent& motion, const DisplacementComponent& disp, HitStateComponent& hit)
{
    if (motion.m_completedLoops > 0)
        hit.m_animationEnd = true;
    if (disp.m_brake || disp.m_lie)
        hit.m_displacementEnd = true;
}
// 受击自然推进。硬直结束时：已死直接进死亡，否则进 HitSwitch。
void updateHit(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
               DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    if (motion.m_completedThisFrame)
        hit.m_animationEnd = true;
    if (disp.m_brake)
        hit.m_displacementEnd = true;
    hit.m_hitTimeMs += dtMs;
    const bool done = hit.m_animationEnd && hit.m_displacementEnd && hit.m_hitTimeMs >= hit.m_stiffTime;
    if (!done)
    {
        state.m_stateMs += dtMs;
        return;
    }
    if (state.m_deathRequested)
    {
        transition(world, entity, state, RoleStateId::Death);
        enterDeath(world, entity, state, hit, motion, disp, identity);
        return;
    }
    transition(world, entity, state, RoleStateId::HitSwitch);
    motion.m_motionName     = role::motionName(RoleStateId::HitSwitch);
    motion.m_timeMs         = {};
    motion.m_completedLoops = 0;
    motion.m_loop           = false;
    motion.m_playing        = true;
}
// 受击恢复：上一帧动画完成则回到待机；已死则进入死亡。
void updateHitSwitch(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                     DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    if (!motion.m_completedThisFrame)
    {
        state.m_stateMs += dtMs;
        return;
    }
    if (state.m_deathRequested)
    {
        transition(world, entity, state, RoleStateId::Death);
        enterDeath(world, entity, state, hit, motion, disp, identity);
        return;
    }
    transition(world, entity, state, RoleStateId::Idle);
    playIdle(motion);
}
// 施法结束回到待机；Attack 内切招结束旧施法并开始新施法。
void updateAttack(World& world, Entity entity, RoleStateComponent& state, SkillCastComponent& cast, MotionStateComponent& motion,
                  const IntentComponent& intent, Fixed dtMs)
{
    if (cast.m_switch)
    {
        cast.m_switch = false;
        skill::rules::endCast(world, entity, true);
        if (cast.m_skill != kNoSkill)
        {
            state.m_stateMs += dtMs;
            return;
        }
    }
    if (cast.m_skill == kNoSkill)
    {
        if (cast.m_nextRun && intent.m_move.m_mode == MoveMode::Run)
        {
            cast.m_nextRun = false;
            transition(world, entity, state, RoleStateId::Run);
            motion.m_motionName     = role::motionName(RoleStateId::Run);
            motion.m_timeMs         = {};
            motion.m_completedLoops = 0;
            motion.m_loop           = true;
            motion.m_rate           = conf::kRunScaleTime;
            motion.m_playing        = true;
            return;
        }
        transition(world, entity, state, RoleStateId::Idle);
        playIdle(motion);
        return;
    }
    state.m_stateMs += dtMs;
}
// 移动 / 待机。
void updateLocomotion(World& world, Entity entity, RoleStateComponent& state, const IntentComponent& intent, MotionStateComponent& motion,
                      DisplacementComponent& disp, Fixed dtMs)
{
    RoleStateId next = state.m_state;
    if (state.m_state == RoleStateId::Idle)
    {
        if (intent.m_move.m_mode == MoveMode::Run)
            next = RoleStateId::Run;
        else if (intent.m_move.m_mode == MoveMode::Walk)
            next = RoleStateId::Walk;
    }
    else if (role::isMoveState(state.m_state))
    {
        if (intent.m_move.m_mode == MoveMode::Stop)
            next = RoleStateId::Idle;
    }
    if (next != state.m_state)
    {
        transition(world, entity, state, next);
        motion.m_motionName = role::motionName(next);
        motion.m_timeMs     = {};
        motion.m_loop       = true;
        motion.m_playing    = true;
        motion.m_rate       = next == RoleStateId::Walk ? conf::kWalkScaleTime
                              : next == RoleStateId::Run ? conf::kRunScaleTime
                                                        : conf::kIdleScaleTime;
        disp.m_velocity     = {};
    }
    else
        state.m_stateMs += dtMs;
    if (state.m_state == RoleStateId::Idle && motion.m_motionName == role::motionName(RoleStateId::HitSwitch))
        playIdle(motion);
}
}  // namespace

// 系统 RoleFsmSystem：4.2 行，见 DESIGN.md §3.2。唯一写 RoleStateComponent 的系统。
// B2 实现 Idle/Walk/Run/Attack/Hit/HitSwitch/Death；HitUp 等受击族后续切片再接。
void runRoleFsm(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().m_dtMs;
    world.ecs().each<RoleStateComponent, IntentComponent, MotionStateComponent, DisplacementComponent, SkillCastComponent, HitStateComponent, IdentityComponent>(
        [&](Entity entity, RoleStateComponent& state, const IntentComponent& intent, MotionStateComponent& motion, DisplacementComponent& disp,
            SkillCastComponent& cast, HitStateComponent& hit, const IdentityComponent& identity) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            state.m_prev = state.m_state;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->m_paused)
                    return;

            // 1. 已在死亡：只维持完成位。进入动作在转移当帧执行。
            if (state.m_state == RoleStateId::Death)
            {
                updateDeath(motion, disp, hit);
                return;
            }
            // 1b. 死亡请求：非受击族立即进入死亡；受击族等自然出口。
            if (state.m_deathRequested && !role::isHitState(state.m_state))
            {
                transition(world, entity, state, RoleStateId::Death);
                enterDeath(world, entity, state, hit, motion, disp, identity);
                return;
            }
            // 木偶同步只提交请求，状态由这里写入。
            if (state.m_puppetRequested)
            {
                const RoleStateId puppet = state.m_puppetState;
                state.m_puppetRequested  = false;
                transition(world, entity, state, puppet);
                return;
            }

            // 3. 已在 Hit：先按完成条件进 HitSwitch（已死则进 Death），本帧不消费待处理受击。
            if (state.m_state == RoleStateId::Hit && hit.m_pendingHitDefId != 0)
            {
                updateHit(world, entity, state, hit, motion, disp, identity, dtMs);
                if (state.m_state == RoleStateId::HitSwitch || state.m_state == RoleStateId::Death)
                    return;
            }
            // 4. 待处理受击。已在 Hit 上追击重播；其余（含 HitSwitch）重新进入并复位计时。
            if (hit.m_pendingHitDefId != 0)
            {
                if (state.m_state == RoleStateId::Hit)
                    replayHit(world, entity, hit, motion);
                else
                {
                    transition(world, entity, state, RoleStateId::Hit);
                    enterHit(world, entity, state, hit, motion);
                }
                return;
            }

            // 5. 挣脱：可挣脱状态下已开始 L 槽施法 → Attack（清位移与硬直）。
            if (role::isCanBreak(state.m_state) && skill::rules::isCastingSlot(world, entity, "L"))
            {
                displacement::rules::reset(world, entity);
                hit.m_stiffTime        = {};
                hit.m_hitTimeMs        = {};
                hit.m_animationEnd     = false;
                hit.m_displacementEnd  = false;
                transition(world, entity, state, RoleStateId::Attack);
                return;
            }

            // 4. 受击族自然推进。完成位是上一帧 AnimationSystem 写入的（R10）。
            if (state.m_state == RoleStateId::Hit)
            {
                updateHit(world, entity, state, hit, motion, disp, identity, dtMs);
                return;
            }
            if (state.m_state == RoleStateId::HitSwitch)
            {
                updateHitSwitch(world, entity, state, hit, motion, disp, identity, dtMs);
                return;
            }

            // 6/7. 施法进入与结束。
            if (state.m_state == RoleStateId::Attack)
            {
                updateAttack(world, entity, state, cast, motion, intent, dtMs);
                return;
            }
            if (cast.m_skill != kNoSkill &&
                (state.m_state == RoleStateId::Idle || role::isMoveState(state.m_state)))
            {
                transition(world, entity, state, RoleStateId::Attack);
                return;
            }

            // 9. 移动 / 待机。
            updateLocomotion(world, entity, state, intent, motion, disp, dtMs);
        });
}
}  // namespace mugen::sim
