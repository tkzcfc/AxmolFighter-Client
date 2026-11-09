// [4.2] RoleFsmSystem
// 读：`RoleState`、`Intent.m_move`、`Health`、`SkillCast`、`HitState`、`MotionState` 完成位、`Displacement` 事件位
// 写：`RoleState`；进入/退出动作写 `MotionState`、`Displacement`
// 消费：`HitState` 待处理受击请求
// 产生：`StateEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 播放受击动作（重放）。
void playHitMotion(MotionState& motion)
{
    motion.m_motionName     = role::motionName(RoleStateId::Hit);
    motion.m_timeMs         = {};
    motion.m_completedLoops = 0;
    motion.m_loop           = false;
    motion.m_rate           = conf::kHitScaleTime;
    motion.m_playing        = true;
}
// 启动受击位移并发出 disp+。
void setHitDisplacement(World& world, Entity entity, HitState& hit)
{
    if (hit.m_displacementId != -1)
    {
        displacement::rules::set(world, entity, hit.m_displacementId);
        DispEvent event;
        event.m_entity         = entity;
        event.m_displacementId = hit.m_displacementId;
        event.m_phase          = 0;
        world.ecs().singleton<EventQueue<DispEvent>>().push(event);
    }
}
// 进入受击状态（首次受击 / 状态切换：复位动画与位移完成位）。
void enterHit(World& world, Entity entity, RoleState& state, HitState& hit, MotionState& motion)
{
    hit.m_hitDefId         = hit.m_pendingHitDefId;
    hit.m_displacementId   = hit.m_pendingDisplacementId;
    hit.m_towardX          = hit.m_pendingTowardX;
    hit.m_pendingHitDefId  = 0;
    hit.m_animationEnd     = false;
    hit.m_displacementEnd  = false;
    hit.m_hitTimeMs        = {};
    playHitMotion(motion);
    setHitDisplacement(world, entity, hit);
    state.m_stateMs = {};
}
// 受击追击重播：保留动画完成位，仅复位位移完成位。
void replayHit(World& world, Entity entity, HitState& hit, MotionState& motion)
{
    hit.m_hitDefId        = hit.m_pendingHitDefId;
    hit.m_displacementId  = hit.m_pendingDisplacementId;
    hit.m_towardX         = hit.m_pendingTowardX;
    hit.m_pendingHitDefId = 0;
    hit.m_displacementEnd = false;
    playHitMotion(motion);
    setHitDisplacement(world, entity, hit);
}
}  // namespace

// 系统 RoleFsmSystem：4.2 行，见 DESIGN.md §3.2。B2 实现 Idle/Walk/Run/Attack/Hit/HitSwitch/Death。
void runRoleFsm(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().m_dtMs;
    world.ecs().each<RoleState, Intent, MotionState, Displacement, SkillCast, HitState, Identity>(
        [&](Entity entity, RoleState& state, const Intent& intent, MotionState& motion, Displacement& disp,
            const SkillCast& cast, HitState& hit, const Identity& identity) {
            if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
                return;
            state.m_prev = state.m_state;

            // 1. 死亡：进入死亡状态体（CombatSystem 直接写状态；此处进入动作与死亡位移）。
            if (state.m_state == RoleStateId::Death)
            {
                if (motion.m_motionName != role::motionName(RoleStateId::Death))
                {
                    motion.m_motionName     = role::motionName(RoleStateId::Death);
                    motion.m_timeMs         = {};
                    motion.m_loop           = false;
                    motion.m_completedLoops = 0;
                    hit.m_animationEnd      = false;
                    hit.m_displacementEnd   = false;
                    // 进入死亡体时清掉上一状态残留的位移事件位，避免首帧误读为位移已结束。
                    disp.m_brake = false;
                    disp.m_lie   = false;
                    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
                    if (role && role->deathDisplacementId > 0)
                    {
                        displacement::rules::set(world, entity, role->deathDisplacementId);
                        DispEvent event;
                        event.m_entity         = entity;
                        event.m_displacementId = role->deathDisplacementId;
                        event.m_phase          = 0;
                        world.ecs().singleton<EventQueue<DispEvent>>().push(event);
                    }
                }
                // 死亡体完成位（R10/R11）：动画完成与位移结束位由上一帧 Animation/Displacement 写入，本帧读取。
                if (motion.m_completedLoops > 0)
                    hit.m_animationEnd = true;
                if (disp.m_brake || disp.m_lie)
                    hit.m_displacementEnd = true;
                return;
            }

            // 2. 消费受击请求：进入 / 追击重播 Hit。
            if (hit.m_pendingHitDefId != 0)
            {
                if (state.m_state == RoleStateId::HitSwitch)
                {
                    state.m_state = RoleStateId::Hit;
                    enterHit(world, entity, state, hit, motion);
                    StateEvent event;
                    event.m_entity = entity;
                    event.m_from   = static_cast<int32_t>(state.m_prev);
                    event.m_to     = static_cast<int32_t>(state.m_state);
                    world.ecs().singleton<EventQueue<StateEvent>>().push(event);
                    return;
                }
                if (state.m_state == RoleStateId::Hit)
                {
                    replayHit(world, entity, hit, motion);
                    return;
                }
                // 非受击族：进入 Hit。
                const auto from  = state.m_state;
                state.m_state    = RoleStateId::Hit;
                enterHit(world, entity, state, hit, motion);
                StateEvent event;
                event.m_entity = entity;
                event.m_from   = static_cast<int32_t>(from);
                event.m_to     = static_cast<int32_t>(state.m_state);
                world.ecs().singleton<EventQueue<StateEvent>>().push(event);
                return;
            }

            // 3. 受击族自然推进。
            if (state.m_state == RoleStateId::Hit)
            {
                // R10/R11：动画完成位与位移事件位由上一帧 AnimationSystem / DisplacementSystem 写入。
                if (motion.m_completedThisFrame)
                    hit.m_animationEnd = true;
                if (disp.m_brake)
                    hit.m_displacementEnd = true;
                hit.m_hitTimeMs += dtMs;
                const bool done = hit.m_animationEnd && hit.m_displacementEnd &&
                                  hit.m_hitTimeMs >= hit.m_stiffTime;
                if (done)
                {
                    state.m_state = RoleStateId::HitSwitch;
                    motion.m_motionName = role::motionName(RoleStateId::HitSwitch);
                    motion.m_timeMs     = {};
                    motion.m_completedLoops = 0;
                    motion.m_loop       = false;
                    StateEvent event;
                    event.m_entity = entity;
                    event.m_from   = static_cast<int32_t>(state.m_prev);
                    event.m_to     = static_cast<int32_t>(state.m_state);
                    world.ecs().singleton<EventQueue<StateEvent>>().push(event);
                    return;
                }
            }
            else if (state.m_state == RoleStateId::HitSwitch)
            {
                // 受击恢复的打断由 AnimationSystem 在同一帧完成（旧实现 motionUpdate 内 abort）。
            }

            // 4. 施法：进入 / 结束 Attack。
            if (state.m_state == RoleStateId::Attack)
            {
                if (cast.m_skill == kNoSkill)
                {
                    state.m_state = RoleStateId::Idle;
                    motion.m_motionName = role::motionName(RoleStateId::Idle);
                    motion.m_timeMs     = {};
                    motion.m_loop       = true;
                    motion.m_rate       = conf::kIdleScaleTime;
                    StateEvent event;
                    event.m_entity = entity;
                    event.m_from   = static_cast<int32_t>(state.m_prev);
                    event.m_to     = static_cast<int32_t>(state.m_state);
                    world.ecs().singleton<EventQueue<StateEvent>>().push(event);
                    return;
                }
                state.m_stateMs += dtMs;
                return;
            }
            if (cast.m_skill != kNoSkill && (state.m_state == RoleStateId::Idle ||
                                             role::isMoveState(state.m_state)))
            {
                const auto from  = state.m_state;
                state.m_state    = RoleStateId::Attack;
                state.m_stateMs  = {};
                StateEvent event;
                event.m_entity = entity;
                event.m_from   = static_cast<int32_t>(from);
                event.m_to     = static_cast<int32_t>(state.m_state);
                world.ecs().singleton<EventQueue<StateEvent>>().push(event);
                return;
            }

            // 5. 移动 / 待机（与 B1 一致）。
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
                const RoleStateId from = state.m_state;
                state.m_state          = next;
                state.m_stateMs        = {};
                motion.m_motionName    = role::motionName(next);
                motion.m_timeMs        = {};
                motion.m_loop          = true;
                motion.m_rate          = next == RoleStateId::Walk   ? conf::kWalkScaleTime
                                         : next == RoleStateId::Run ? conf::kRunScaleTime
                                                                    : conf::kIdleScaleTime;
                disp.m_velocity        = {};
                StateEvent event;
                event.m_entity = entity;
                event.m_from   = static_cast<int32_t>(from);
                event.m_to     = static_cast<int32_t>(next);
                world.ecs().singleton<EventQueue<StateEvent>>().push(event);
            }
            else
                state.m_stateMs += dtMs;

            // 受击恢复后的待机动作切换（HitSwitch 打断后状态已 Idle，动作仍为 hitswitch）。
            if (state.m_state == RoleStateId::Idle && motion.m_motionName == role::motionName(RoleStateId::HitSwitch))
            {
                motion.m_motionName = role::motionName(RoleStateId::Idle);
                motion.m_timeMs     = {};
                motion.m_loop       = true;
                motion.m_rate       = conf::kIdleScaleTime;
            }
        });
}
}  // namespace mugen::sim
