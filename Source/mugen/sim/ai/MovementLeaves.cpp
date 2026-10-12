#include "MovementLeaves.h"
#include "AiAgent.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/core/math/FixedMath.h"

namespace mugen::sim::ai::movement
{
namespace
{
// 更新循环动画时同时复位动作时间。
void play(behavior::BTCtx& ctx, behavior::ActionType type)
{
    auto& motion = ctx.world.ecs().get<MotionState>(ctx.entity);
    motion.motionName = behavior::animationName(type);
    motion.timeMs = {};
    motion.m_completedLoops = 0;
    motion.loop = motion.playing = true;
    motion.actionScale = type == behavior::ActionType::Idle ? conf::kIdleScaleTime : conf::kWalkScaleTime;
}
// 到达后清速度并从零开始停留，不能把行走时间算入停留。
void arrive(behavior::BTCtx& ctx, ActionMove& action)
{
    action.m_complete = true;
    action.m_timeMs = {};
    ctx.world.ecs().get<RoleHitState>(ctx.entity).forceVelocity = {};
    play(ctx, behavior::ActionType::Idle);
}
// 单帧接近量使用固定逻辑帧长；实际积分仍由位移系统负责。
void advance(behavior::BTCtx& ctx, ActionMove& action)
{
    if (action.m_complete) return;
    auto& ecs = ctx.world.ecs();
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ecs.get<RoleIdentity>(ctx.entity).roleId);
    if (!row) return;
    const auto goal = ecs.get<RoleStatus>(ctx.entity).entityParams.move_pos;
    auto& transform = ecs.get<Transform>(ctx.entity);
    const Fixed x = goal.x - transform.position.x, z = goal.z - transform.position.z;
    const Fixed speed = row->velocity * conf::kWalkRate;
    const Fixed distance = speed * kLogicDtMs;
    if (-distance <= x && x <= distance && -distance <= z && z <= distance)
    {
        transform.position.x = goal.x;
        transform.position.z = goal.z;
        arrive(ctx, action);
        return;
    }
    const auto angle = FixedMath::atan2Deg(z, x);
    auto& velocity = ecs.get<RoleHitState>(ctx.entity).forceVelocity;
    velocity.x = FixedMath::cosDeg(angle) * speed;
    velocity.z = FixedMath::sinDeg(angle) * speed;
    const auto* target = ecs.tryGet<Transform>(ecs.get<SkillPool>(ctx.entity).m_target);
    if (target)
    {
        transform.towardX = target->position.x > transform.position.x ? 1 : -1;
        transform.vectorX = Fixed::fromInt(transform.towardX);
        transform.vectorZ = target->position.z > transform.position.z ? "1"_fx : "-1"_fx;
    }
}
// 根据叶状态所在的 DFS 节点登记位移监听器。
int32_t nodeFor(behavior::BTCtx& ctx, behavior::LeafState& state)
{
    auto& tree = ctx.world.ecs().get<RoleTree>(ctx.entity);
    const auto& topology = behavior::definition(tree.defKey).topology();
    for (uint32_t i = 0; i < topology.nodes().size(); ++i)
        if (topology.node(i).type == BTNodeType::Action && &tree.instance.leaves[topology.leafIndex(i)] == &state)
            return static_cast<int32_t>(i);
    return -1;
}
}

void enter(behavior::BTCtx& ctx, const behavior::Param& param, behavior::LeafState& state)
{
    state = ActionMove{};
    const auto type = std::get<behavior::ActionType>(param);
    behavior::behaviorStateStart(ctx.world, ctx.entity, type);
    auto& action = std::get<ActionMove>(state);
    auto& ecs = ctx.world.ecs();
    const auto goal = ecs.get<RoleStatus>(ctx.entity).entityParams.move_pos;
    const auto& ranges = ecs.singleton<MapRegion>().moveRanges;
    bool inside = ranges.empty();
    for (const auto& range : ranges)
        inside = inside || (range.x <= goal.x && goal.x <= range.x + range.width &&
                            range.y <= goal.z && goal.z <= range.y + range.height);
    if (!inside) { action.m_complete = true; return; }
    displacement::set(ctx.world, ctx.entity, -1, nodeFor(ctx, state));
    const auto* row = conf::Config::instance().find<conf::AiConfig>(ecs.get<RoleIdentity>(ctx.entity).aiId);
    if (row)
    {
        const auto& delay = type == behavior::ActionType::Patrol ? row->patrolDelayTime :
                            type == behavior::ActionType::Chase ? row->chaseDelayTime : row->alertDelayTime;
        if (delay.size() >= 2)
            action.m_delayMs = ecs.singleton<RngState>().random.randomF(Fixed::fromInt(delay[0]), Fixed::fromInt(delay[1]));
    }
    play(ctx, type);
    advance(ctx, action);
}

BTStatus tick(behavior::BTCtx& ctx, const behavior::Param& param, behavior::LeafState& state)
{
    auto& action = std::get<ActionMove>(state);
    action.m_timeMs += ctx.world.ecs().singleton<WorldClock>().dtMs;
    const auto type = std::get<behavior::ActionType>(param);
    if (type == behavior::ActionType::Patrol)
    {
        if (checkTarget(ctx.world, ctx.entity) == Result::Success) return BTStatus::Success;
    }
    else if (status::isAttackStatus(ctx.world.ecs().get<RoleStatus>(ctx.entity))) return BTStatus::Success;
    advance(ctx, action);
    return action.m_complete && action.m_timeMs > action.m_delayMs ? BTStatus::Success : BTStatus::Running;
}

void exit(behavior::BTCtx& ctx, const behavior::Param& param, behavior::LeafState&)
{
    ctx.world.ecs().get<Displacement>(ctx.entity).listenerNode = -1;
    ctx.world.ecs().get<RoleHitState>(ctx.entity).forceVelocity = {};
    behavior::behaviorStateEnd(ctx.world, ctx.entity, std::get<behavior::ActionType>(param));
}

bool onDisplacementEvent(behavior::BTCtx& ctx, const behavior::Param&, behavior::LeafState& state)
{
    if (ctx.event == DisplacementEvent::Left || ctx.event == DisplacementEvent::Right ||
        ctx.event == DisplacementEvent::Top || ctx.event == DisplacementEvent::Bottom)
    {
        auto& action = std::get<ActionMove>(state);
        if (!action.m_complete) arrive(ctx, action);
    }
    return false;
}
}
