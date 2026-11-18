#include "mugen/sim/role/RoleFsmRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::role::rules
{
// 唯一状态写入：更新状态、清零计时并发出带正确 from 的 StateEvent。
void transition(World& world, Entity entity, RoleStateComponent& state, RoleStateId to)
{
    if (state.state == to)
        return;
    const RoleStateId from = state.state;
    BuffContext ended;
    ended.target = entity;
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::BehaviorStateEnd, ended);
    state.state   = to;
    state.stateMs = {};
    BuffContext began;
    began.target = entity;
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::BehaviorStateStart, began);
    StateEvent event;
    event.entity = entity;
    event.from   = static_cast<int32_t>(from);
    event.to     = static_cast<int32_t>(to);
    world.ecs().singleton<EventQueue<StateEvent>>().push(event);
}

// 播放待机动作。
void playIdle(MotionStateComponent& motion)
{
    motion.motionName     = role::motionName(RoleStateId::Idle);
    motion.timeMs         = {};
    motion.completedLoops = 0;
    motion.loop           = true;
    motion.rate           = conf::kIdleScaleTime;
    motion.playing        = true;
}

// 施法结束回到待机或跑动。
void exitAttack(World& world, Entity entity, RoleStateComponent& state, SkillCastComponent& cast,
                MotionStateComponent& motion, const IntentComponent& intent)
{
    if (cast.nextRun && intent.move.mode == MoveMode::Run)
    {
        cast.nextRun = false;
        transition(world, entity, state, RoleStateId::Run);
        motion.motionName     = role::motionName(RoleStateId::Run);
        motion.timeMs         = {};
        motion.completedLoops = 0;
        motion.loop           = true;
        motion.rate           = conf::kRunScaleTime;
        motion.playing        = true;
        return;
    }
    transition(world, entity, state, RoleStateId::Idle);
    playIdle(motion);
}
}  // namespace mugen::sim::role::rules
