#include "AiMoveState.h"

#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/FixedMath.h"
#include "mugen/sim/World.h"

namespace mugen::sim::role::brain::states
{
namespace
{
// 更新循环动画时同时复位动作时间。
void play(RoleContext& ctx, behavior::ActionType type)
{
    auto& motion         = ctx.get<MotionState>();
    motion.motionName    = behavior::animationName(type);
    motion.timeMs        = {};
    motion.m_completedLoops = 0;
    motion.loop = motion.playing = true;
    motion.actionScale = type == behavior::ActionType::Idle ? conf::kIdleScaleTime : conf::kWalkScaleTime;
}
// 到达后清速度并从零开始停留，不能把行走时间算入停留。
void arrive(RoleContext& ctx, ai::ActionMove& action)
{
    action.m_complete = true;
    action.m_timeMs   = {};
    ctx.get<RoleHitState>().forceVelocity = {};
    play(ctx, behavior::ActionType::Idle);
}
// 单帧接近量使用固定逻辑帧长；实际积分仍由位移系统负责。
void advance(RoleContext& ctx, ai::ActionMove& action)
{
    if (action.m_complete)
        return;
    auto& ecs    = ctx.world().ecs();
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ctx.get<RoleIdentity>().roleId);
    if (!row)
        return;
    const auto goal = ctx.get<RoleStatus>().entityParams.move_pos;
    auto& transform = ctx.get<Transform>();
    const Fixed x = goal.x - transform.position.x, z = goal.z - transform.position.z;
    const Fixed speed    = row->velocity * conf::kWalkRate;
    const Fixed distance = speed * kLogicDtMs;
    if (-distance <= x && x <= distance && -distance <= z && z <= distance)
    {
        transform.position.x = goal.x;
        transform.position.z = goal.z;
        arrive(ctx, action);
        return;
    }
    const auto angle = FixedMath::atan2Deg(z, x);
    auto& velocity   = ctx.get<RoleHitState>().forceVelocity;
    velocity.x       = FixedMath::cosDeg(angle) * speed;
    velocity.z       = FixedMath::sinDeg(angle) * speed;
    const auto* target = ecs.tryGet<Transform>(ecs.get<SkillPool>(ctx.entity()).m_target);
    if (target)
    {
        transform.towardX = target->position.x > transform.position.x ? 1 : -1;
        transform.vectorX = Fixed::fromInt(transform.towardX);
        transform.vectorZ = target->position.z > transform.position.z ? "1"_fx : "-1"_fx;
    }
}
}  // namespace

void AiMove::enter(RoleContext& ctx, Param type, State& state)
{
    state = ai::ActionMove{};
    ctx.notifyActionStart(type);
    auto& ecs   = ctx.world().ecs();
    const auto goal = ctx.get<RoleStatus>().entityParams.move_pos;
    const auto& ranges = ecs.singleton<MapRegion>().moveRanges;
    bool inside = ranges.empty();
    for (const auto& range : ranges)
        inside = inside || (range.x <= goal.x && goal.x <= range.x + range.width &&
                            range.y <= goal.z && goal.z <= range.y + range.height);
    if (!inside)
    {
        state.m_complete = true;
        return;
    }
    displacement::DisplacementDriver(ctx.world(), ctx.entity()).set(-1, ctx.node());
    const auto* row = conf::Config::instance().find<conf::AiConfig>(ctx.get<RoleIdentity>().aiId);
    if (row)
    {
        const auto& delay = type == behavior::ActionType::Patrol ? row->patrolDelayTime :
                            type == behavior::ActionType::Chase ? row->chaseDelayTime : row->alertDelayTime;
        if (delay.size() >= 2)
            state.m_delayMs = ecs.singleton<RngState>().random.randomF(Fixed::fromInt(delay[0]), Fixed::fromInt(delay[1]));
    }
    play(ctx, type);
    advance(ctx, state);
}

BTStatus AiMove::update(RoleContext& ctx, Param, State& state)
{
    state.m_timeMs += ctx.dtMs();
    if (ctx.get<RoleStatus>().isAttack())
        return BTStatus::Success;
    advance(ctx, state);
    return state.m_complete && state.m_timeMs > state.m_delayMs ? BTStatus::Success : BTStatus::Running;
}

void AiMove::exit(RoleContext& ctx, Param type, State&)
{
    ctx.get<Displacement>().listenerNode = -1;
    ctx.get<RoleHitState>().forceVelocity = {};
    ctx.notifyActionEnd(type);
}

bool AiMove::onDisplacement(RoleContext& ctx, Param, State& state)
{
    if (ctx.event() == DisplacementEvent::Left || ctx.event() == DisplacementEvent::Right ||
        ctx.event() == DisplacementEvent::Top || ctx.event() == DisplacementEvent::Bottom)
    {
        if (!state.m_complete)
            arrive(ctx, state);
    }
    return false;
}
}  // namespace mugen::sim::role::brain::states
