#include "mugen/sim/role/RoleFsmRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::role::rules
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

// 施法结束回到待机或跑动。
void exitAttack(World& world, Entity entity, RoleStateComponent& state, SkillCastComponent& cast,
                MotionStateComponent& motion, const IntentComponent& intent)
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
}
}  // namespace mugen::sim::role::rules
