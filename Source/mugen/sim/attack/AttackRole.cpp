#include "AttackRole.h"
#include "ActionAttack.h"
#include "AttackSeams.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/brain/RoleBrain.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/skill/SkillTriggers.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::attack
{
namespace
{
// 从攻击叶参数查询对应的动作配置。
const conf::ActionConfig* config(const AttackLeafParam& param)
{
    return conf::Config::instance().find<conf::ActionConfig>(param.m_actionId);
}
/// 返回当前连段首段的技能配置 id。
int32_t baseSkill(const SkillPool& pool, int32_t index)
{
    const auto& source = pool.m_skills.at(index);
    const auto first = skill::getGivenSkill(pool, pool.m_slots.at(source.m_address.m_slot).m_name,
        source.m_address.m_index, source.m_address.m_mode, 1, true);
    return first == kNoSkill ? source.m_configId : pool.m_skills.at(first).m_configId;
}
/// 在普通或额外打断窗口内接续技能或跑动。
void interrupt(World& world, Entity entity, int32_t index, bool extra, ActionAttack& action)
{
    auto& pool = world.ecs().get<SkillPool>(entity);
    auto& source = pool.m_skills.at(index);
    auto next = source.m_nextSkill;
    if (extra) pool.m_allowExtraInterrupt = true;
    if (next != kNoSkill)
    {
        const auto& candidate = pool.m_skills.at(next);
        if (candidate.m_group == 2 && !pool.m_crazy)
            next = skill::getGivenSkill(pool, skill::baseSlot(pool, candidate).substr(0, 1));
        if (next != kNoSkill)
        {
            const auto& target = pool.m_skills.at(next);
            const bool sameSlot = target.m_address.m_slot == source.m_address.m_slot;
            bool allow = sameSlot || skill::isPriority(target, source);
            if (extra)
            {
                const auto& types = conf::Config::instance().get<conf::SkillAttackConfig>(target.m_configId).sorderControlType;
                allow = sameSlot ? (target.m_address != source.m_address &&
                    std::find(types.begin(), types.end(), 2) != types.end()) : skill::isSuperPriority(pool, next, index);
            }
            if (allow && skill::dealWithNextSkillBase(world, entity)) action.m_finished = true;
        }
    }
    else if (source.m_nextRun)
    {
        source.m_nextRun = false;
        action.m_finished = true;
        world.ecs().get<RoleStatus>(entity).mix(EntityRoleStatus::Run);
    }
    if (!extra) pool.m_allowInterrupt = true;
}
}  // namespace

/// 初始化攻击动作并应用其开始帧副作用。
void AttackAction::enter(role::brain::RoleContext& ctx, Param source, State& leaf)
{
    Fixed resetAt;
    bool wasStatic = false;
    { resetAt = leaf.m_staticResetAt; wasStatic = leaf.m_static; }
    leaf = AttackRole{};
    leaf.m_staticResetAt = resetAt;
    leaf.m_static = wasStatic;
    const auto* row = config(source);
    if (!row) { leaf.m_action.m_finished = true; return; }
    auto& world = ctx.world();
    auto& ecs = world.ecs();
    const auto entity = ctx.entity();
    auto& pool = ecs.get<SkillPool>(entity);
    pool.m_allowInterrupt = pool.m_allowExtraInterrupt = false;
    buff::trigger(world, entity, BFEvent::BehaviorStateStart, {source.m_skill, {}, row->id});
    if (row->control == 1) dealWithDirection(world, entity, true);
    auto& transform = ecs.get<Transform>(entity);
    if (row->relativeActionPos.size() >= 3)
    {
        const auto& p = row->relativeActionPos;
        if (row->actionPosType == 1) transform.position = {p[0], p[1], p[2]};
        else if (row->actionPosType == 0 && !ecs.singleton<MapRegion>().moveRanges.empty())
        {
            const auto& r = ecs.singleton<MapRegion>().moveRanges.front();
            transform.position = {r.x + r.width * p[0], p[1], r.y + r.height * p[2]};
        }
    }
    if (row->actionOrientation == 0) transform.towardX = -1;
    else if (row->actionOrientation == 1) transform.towardX = 1;
    transform.vectorX = Fixed::fromInt(transform.towardX);
    buff::trigger(world, entity, BFEvent::BeforeRoleSkillAction, {source.m_skill, {}, row->id});
    auto& motion = ecs.get<MotionState>(entity);
    motion.actionScale = row->actionScaleTime * ecs.get<RoleHitState>(entity).m_actionScale;
    MG_ASSERT(motion.actionScale > Fixed{} && "Attack animation scale must be positive");
    leaf.m_action.m_interval = "1000"_fx * kLogicDtSec / motion.actionScale;
    motion.entryId.clear();
    if (const auto* name = std::get_if<std::string>(&row->action)) motion.motionName = *name;
    else motion.motionName = std::to_string(std::get<int32_t>(row->action));
    motion.timeMs = {};
    motion.m_completedLoops = 0;
    motion.loop = row->loop == "-1"_fx || row->loop > "1"_fx;
    motion.playing = true;
    displacement::reset(world, entity);
    displacement::set(world, entity, row->displacementId, ctx.node());
    const auto first = baseSkill(pool, source.m_skill);
    for (const auto id : row->buffIds) if (id != -1) buff::addActionBuff(world, entity, id, first);
    if (row->ghost != -1) world.emit(entity, Ghost{"-1"_fx});
    if (row->tipsShow != -1) world.emit(entity, Tips{std::to_string(row->tipsShow)});
    if (row->dialogShow != -1) world.emit(entity, Bubble{row->dialogShow});
    world.emit(entity, Sound{0, row->soundId, false, row->id});
    /// 三个数组分别与效果和镜头配置保持相同下标。
    leaf.m_action.m_effects.resize(row->effectIds.size());
    leaf.m_action.m_spawned.resize(row->effectIds.size());
    leaf.m_action.m_shaken.resize(row->cameraFrame.size());
}

/// 推进攻击动作的时序事件并在完成时结束叶子。
BTStatus AttackAction::update(role::brain::RoleContext& ctx, Param source, State& leaf)
{
    auto& action = leaf.m_action;
    const auto* row = config(source);
    if (!row) return BTStatus::Success;
    auto& world = ctx.world();
    auto& ecs = world.ecs();
    const auto entity = ctx.entity();
    action.m_time += ecs.singleton<WorldClock>().dtMs;
    const auto at = [&](int32_t frame) { return action.m_interval * Fixed::fromInt(frame); };
    if (!leaf.m_displayed && action.m_time >= at(row->displaySpineFrame))
    {
        leaf.m_displayed = true;
        world.emit(entity, DisplaySpine{0, row->displaySpineIds});
    }
    dealWithTime(action, *row);
    if (ecs.get<RoleIdentity>(entity).roleType == 1 && ecs.get<RoleHitState>(entity).m_hitProtectExecuted)
    {
        action.m_finished = true;
        ecs.get<RoleStatus>(entity).mix(EntityRoleStatus::HitDown);
    }
    /// 普通窗口先开放，额外窗口仅在动作尚未结束时尝试。
    if (row->interruptFrame != -1 && action.m_time > at(row->interruptFrame))
        interrupt(world, entity, source.m_skill, false, action);
    if (!action.m_finished && row->interruptExtraFrame != -1 && action.m_time > at(row->interruptExtraFrame))
        interrupt(world, entity, source.m_skill, true, action);
    dealWithControl(world, entity, *row);
    const auto& pool = ecs.get<SkillPool>(entity);
    for (size_t i = 0; i < row->effectIds.size(); ++i)
    {
        const auto* id = std::get_if<int32_t>(&row->effectIds[i]);
        if (!id || *id == -1 || action.m_spawned[i] || i >= row->effectFrames.size() || action.m_time < at(row->effectFrames[i])) continue;
        effect::RoleSpawn request{*id, pool.m_skills.at(source.m_skill).m_configId,
            baseSkill(pool, source.m_skill), pool.m_skills.at(source.m_skill).m_address.m_slot, row->extendRoleVec == 1};
        if (row->customVec.size() >= 3)
            request.m_vector = {Fixed::fromInt(row->customVec[0]), Fixed::fromInt(row->customVec[1]), Fixed::fromInt(row->customVec[2])};
        action.m_spawned[i] = 1;
        action.m_effects[i] = effect::spawnFromRole(world, entity, request);
    }
    dealWithShake(world, entity, action, *row);
    if (!leaf.m_transformLogged && row->transformId != -1 && action.m_time >= at(row->transformFrame) &&
        !ecs.get<RoleStatus>(entity).isDeath())
    {
        leaf.m_transformLogged = true;
        MG_LOG_W("Attack transformation is not implemented; skipping action {}", row->id);
    }
    const auto now = ecs.singleton<WorldClock>().elapsedMs;
    if (leaf.m_static && now >= leaf.m_staticResetAt) leaf.m_static = false;
    if (!leaf.m_static && row->staticTarget >= 0 && row->staticTarget <= 2 && action.m_time >= at(row->staticStartFrame))
    {
        const auto duration = row->staticTarget == 2 ? Fixed::fromInt(row->displaySpineFrameCount) * kLogicDtSec * "1000"_fx : Fixed::fromInt(row->staticTime);
        ecs.each<RoleIdentity, RoleTimers>([&](Entity target, const RoleIdentity& identity, RoleTimers& timers) {
            if ((target == entity && row->staticTarget != 0) || identity.camp == static_cast<int32_t>(EntityCamp::Enemy) ||
                identity.camp == static_cast<int32_t>(EntityCamp::EnemyFriend)) timers.staticMs = std::max(timers.staticMs, duration);
        });
        leaf.m_static = true;
        leaf.m_staticResetAt = now + Fixed::fromInt(row->staticResetTime);
    }
    return action.m_finished ? BTStatus::Success : BTStatus::Running;
}

/// 撤销攻击动作施加的临时资源与表现状态。
void AttackAction::exit(role::brain::RoleContext& ctx, Param source, State& leaf)
{
    const auto* row = config(source);
    if (!row) return;
    auto& world = ctx.world();
    const auto entity = ctx.entity();
    buff::trigger(world, entity, BFEvent::BehaviorStateEnd, {source.m_skill, {}, row->id});
    clearEffects(world, entity, leaf.m_action);
    buff::trigger(world, entity, BFEvent::AfterRoleSkillAction, {source.m_skill, {}, row->id});
    auto& pool = world.ecs().get<SkillPool>(entity);
    pool.m_allowInterrupt = true;
    pool.m_allowExtraInterrupt = false;
    world.ecs().get<MotionState>(entity).actionScale = conf::kIdleScaleTime;
    displacement::reset(world, entity);
    for (const auto id : row->buffIds) if (id != -1) buff::removeActionBuff(world, entity, id);
    if (row->ghost != -1) world.emit(entity, Ghost{});
    world.emit(entity, Sound{0, {}, true, row->id});
}

/// 由行为树分派当前攻击的位移事件。
bool AttackAction::onDisplacement(role::brain::RoleContext& ctx, Param source, State& leaf)
{
    const auto* row = config(source);
    if (!row) return false;
    return displacementEvent(ctx.world(), ctx.entity(), leaf.m_action, *row, ctx.event());
}

// 同步动画循环结束到当前施法中的攻击叶。
void animationComplete(World& world, Entity entity, int64_t loops)
{
    auto& brain = world.ecs().get<RoleBrain>(entity);
    auto& plan  = brain.m_cast;
    if (brain.m_branch != role::brain::RoleBranch::Attack || plan.m_skill < 0 || plan.m_index < 0 ||
        plan.m_index >= static_cast<int32_t>(plan.m_actions.size()))
        return;
    const auto* row = conf::Config::instance().find<conf::ActionConfig>(plan.m_actions[static_cast<size_t>(plan.m_index)]);
    if (row)
        completeAnimation(plan.m_leaf.m_action, *row, loops);
}
}  // namespace mugen::sim::attack
