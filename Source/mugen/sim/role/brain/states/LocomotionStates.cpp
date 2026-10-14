#include "LocomotionStates.h"

#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/FixedMath.h"
#include "mugen/sim/World.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim::role::brain::states
{
namespace
{
using S = EntityRoleStatus;
// 为行走和跑动叶子发出配置的脚步声。
void footstep(RoleContext& ctx, behavior::ActionType type)
{
    if (type != behavior::ActionType::Walk && type != behavior::ActionType::Run)
        return;
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ctx.get<RoleIdentity>().roleId);
    const size_t index = type == behavior::ActionType::Walk ? 0 : 1;
    if (row && index < row->soundId.size() && row->soundId[index] != -1)
        ctx.world().emit(ctx.entity(), Sound{row->soundId[index]});
}
}  // namespace

void Locomotion::enter(RoleContext& ctx, Param type, State& state)
{
    state = behavior::ActionState{};
    ctx.notifyActionStart(type);
    auto& motion                 = ctx.get<MotionState>();
    motion.motionName            = behavior::animationName(type);
    motion.timeMs                = {};
    motion.m_completedLoops      = 0;
    motion.loop                  = true;
    motion.playing               = true;
    motion.actionScale           = type == behavior::ActionType::Walk  ? conf::kWalkScaleTime
                                   : type == behavior::ActionType::Run ? conf::kRunScaleTime
                                                                       : conf::kIdleScaleTime;
    if (type == behavior::ActionType::Idle)
        displacement::DisplacementDriver(ctx.world(), ctx.entity()).reset();
    footstep(ctx, type);
}

BTStatus Locomotion::update(RoleContext& ctx, Param type, State& state)
{
    auto& ecs = ctx.world().ecs();
    state.fTime += ecs.singleton<WorldClock>().dtMs;
    const auto& motion = ecs.get<MotionState>(ctx.entity());
    if (motion.timeMs < state.lastMotionTime)
        footstep(ctx, type);
    state.lastMotionTime = motion.timeMs;
    if (ctx.get<RoleIdentity>().roleType == 1 && ctx.get<RoleHitState>().m_hitProtectExecuted)
    {
        ctx.get<RoleStatus>().mix(S::HitDown);
        return BTStatus::Success;
    }
    if (type == behavior::ActionType::Idle)
        return BTStatus::Running;
    const auto command = ctx.get<RoleControl>().m_last;
    auto& t            = ctx.get<Transform>();
    auto& v            = ctx.get<RoleHitState>().forceVelocity;
    if (command.quadrant == 0)
    {
        v.x = v.z = {};
        return BTStatus::Running;
    }
    const Fixed angle = Fixed::ratio(command.angle, 1000);
    if (command.angle != 90000)
    {
        t.towardX = command.quadrant <= 2 ? 1 : -1;
        t.vectorX = Fixed::fromInt(t.towardX);
    }
    if (command.angle != 0)
        t.vectorZ = Fixed::fromInt(command.quadrant == 1 || command.quadrant == 4 ? 1 : -1);
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ctx.get<RoleIdentity>().roleId);
    if (row)
    {
        const Fixed speed = row->velocity * (type == behavior::ActionType::Run ? conf::kRunRate : conf::kWalkRate);
        v.x               = FixedMath::cosDeg(angle) * speed * t.vectorX;
        v.z               = FixedMath::sinDeg(angle) * speed * t.vectorZ;
    }
    return BTStatus::Running;
}

void Locomotion::exit(RoleContext& ctx, Param type, State&)
{
    ctx.notifyActionEnd(type);
}

void Jostled::enter(RoleContext& ctx, Param, State& state)
{
    const auto id = ctx.get<RoleStatus>().entityParams.displacement_id;
    state         = behavior::JostledState{id, false};
    ctx.notifyActionStart(behavior::ActionType::Jostled);
    auto& motion            = ctx.get<MotionState>();
    motion.motionName       = behavior::animationName(behavior::ActionType::Jostled);
    motion.timeMs           = {};
    motion.m_completedLoops = 0;
    motion.loop = motion.playing = true;
    motion.actionScale           = conf::kWalkScaleTime;
    const auto* row = conf::Config::instance().find<conf::DisplacementConfig>(id);
    if (row && !row->velocity.empty() && row->velocity[0] != Fixed{})
        ctx.get<Transform>().towardX *= -1;
    displacement::DisplacementDriver(ctx.world(), ctx.entity()).set(id, ctx.node());
}

BTStatus Jostled::update(RoleContext&, Param, State& state)
{
    return state.m_complete ? BTStatus::Success : BTStatus::Running;
}

void Jostled::exit(RoleContext& ctx, Param, State&)
{
    ctx.notifyActionEnd(behavior::ActionType::Jostled);
    displacement::DisplacementDriver(ctx.world(), ctx.entity()).reset();
}

bool Jostled::onDisplacement(RoleContext& ctx, Param, State& state)
{
    if (ctx.event() == DisplacementEvent::Brake && !state.m_complete)
    {
        state.m_complete = true;
        const auto* row = conf::Config::instance().find<conf::DisplacementConfig>(state.m_displacementId);
        if (row && !row->velocity.empty() && row->velocity[0] != Fixed{})
            ctx.get<Transform>().vectorX = -ctx.get<Transform>().vectorX;
    }
    return false;
}

void Finding::enter(RoleContext& ctx, Param, State& state)
{
    const auto type = ctx.world().setup().mode == WorldMode::Town ? behavior::ActionType::Run : behavior::ActionType::Walk;
    Locomotion::enter(ctx, type, state);
    displacement::DisplacementDriver(ctx.world(), ctx.entity()).reset();
}

BTStatus Finding::update(RoleContext& ctx, Param, State&)
{
    auto& ecs = ctx.world().ecs();
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ctx.get<RoleIdentity>().roleId);
    if (!row)
        return BTStatus::Success;
    auto& transform = ctx.get<Transform>();
    auto& params    = ctx.get<RoleStatus>().entityParams;
    const auto goal = params.m_path.empty() ? params.move_pos : params.m_path.front();
    const auto x = goal.x - transform.position.x, z = goal.z - transform.position.z;
    const auto distance = FixedMath::sqrt(x * x + z * z);
    const auto speed = row->velocity * (ctx.world().setup().mode == WorldMode::Town ? conf::kRunRate : conf::kWalkRate);
    const auto angle = FixedMath::atan2Deg(z, x);
    auto& velocity   = ctx.get<RoleHitState>().forceVelocity;
    velocity.x       = FixedMath::cosDeg(angle) * speed;
    velocity.z       = FixedMath::sinDeg(angle) * speed;
    const auto step  = Fixed::ratio(1000, 60) * FixedMath::sqrt(velocity.x * velocity.x + velocity.z * velocity.z * conf::kZRate * conf::kZRate);
    if (distance <= step)
    {
        transform.position.x = goal.x;
        transform.position.z = goal.z;
        velocity             = {};
        if (!params.m_path.empty())
        {
            params.m_path.erase(params.m_path.begin());
            if (!params.m_path.empty())
                return BTStatus::Running;
        }
        auto& motion         = ctx.get<MotionState>();
        motion.motionName    = "stand";
        motion.timeMs        = {};
        motion.actionScale   = conf::kIdleScaleTime;
        return BTStatus::Success;
    }
    if (x != Fixed{})
        transform.towardX = x < Fixed{} ? -1 : 1;
    transform.vectorX = Fixed::fromInt(transform.towardX);
    transform.vectorZ = z < Fixed{} ? "-1"_fx : "1"_fx;
    return BTStatus::Running;
}

void Finding::exit(RoleContext& ctx, Param, State& state)
{
    const auto type = ctx.world().setup().mode == WorldMode::Town ? behavior::ActionType::Run : behavior::ActionType::Walk;
    Locomotion::exit(ctx, type, state);
    displacement::DisplacementDriver(ctx.world(), ctx.entity()).reset();
}
}  // namespace mugen::sim::role::brain::states
