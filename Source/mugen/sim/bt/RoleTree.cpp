#include "RoleTree.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/core/math/FixedMath.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/core/bt/BTDefCache.h"
#include "mugen/sim/attack/AttackRole.h"
#include "HitActions.h"
#include "mugen/sim/ai/MovementLeaves.h"

namespace mugen::sim::behavior
{
namespace
{
using S = EntityRoleStatus;
/// 将技能控制命令转换为方向角并交给技能系统。
void skillEvent(World& world, Entity entity, const PlayerCommand& command)
{
    auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (!pool)
        return;
    if (command.quadrant == 0)
        pool->m_controlAngle.reset();
    else
    {
        const auto angle     = Fixed::ratio(command.angle, 1000);
        pool->m_controlAngle = command.quadrant == 1   ? "90"_fx - angle
                               : command.quadrant == 2 ? "90"_fx + angle
                               : command.quadrant == 3 ? "270"_fx - angle
                                                       : "270"_fx + angle;
    }
    skill::dealButtonEvent(world, entity, command.controlEvent, command.pressType == 2);
}
/// 在攻击守卫退出时复位当前技能。
void resetSkill(World& world, Entity entity)
{
    skill::resetSkill(world, entity);
}
/// 返回当前技能；未选择技能时为空。
const SkillBase* currentSkill(const SkillPool& pool)
{
    return pool.m_current == kNoSkill ? nullptr : &pool.m_skills.at(pool.m_current);
}
/// 验证攻击条件或角色状态守卫。
bool check(BTCtx& ctx, const Param& param)
{
    if (std::holds_alternative<AttackCondition>(param))
    {
        const auto condition = std::get<AttackCondition>(param);
        const auto& pool     = ctx.world.ecs().get<SkillPool>(ctx.entity);
        const auto& source   = pool.m_skills.at(condition.skill);
        const auto* current  = currentSkill(pool);
        switch (condition.type)
        {
        case AttackConditionType::Slot:
            return current && current->m_address.m_slot == source.m_address.m_slot;
        case AttackConditionType::SkillStep:
            return current && current->m_address == source.m_address;
        case AttackConditionType::Pipe:
            return current && current->m_address == source.m_address && current->m_pipe == condition.pipe;
        case AttackConditionType::VectorIndex:
            return current && current->m_toward == condition.toward;
        case AttackConditionType::Press:
            return current && !skill::canUpRelease(*current);
        case AttackConditionType::Release:
            return current && skill::canUpRelease(*current);
        }
    }
    const auto guard = std::get<Guard>(param);
    const auto& r    = ctx.world.ecs().get<RoleStatus>(ctx.entity);
    if (!status::isStatus(r, guard.status))
        return false;
    switch (guard.status)
    {
    case S::Alert:
        return !status::isTobeHitStatus(r);
    case S::Run:
    case S::Walk:
        return !status::isAttackStatus(r) && !status::isJostledStatus(r);
    case S::Idle:
        if (guard.city)
            return !status::isMoveStatus(r);
        return !status::isAttackStatus(r) && !status::isWakeStatus(r) && !status::isTobeHitStatus(r) &&
               !status::isMoveStatus(r) && !status::isJostledStatus(r);
    default:
        return true;
    }
}
/// 在攻击条件进入时更新施放管线或按住状态。
void condEnter(BTCtx& ctx, const Param& param)
{
    if (!std::holds_alternative<AttackCondition>(param))
        return;
    const auto condition = std::get<AttackCondition>(param);
    if (condition.type == AttackConditionType::Pipe)
        skill::dealWithCastPipeBegan(ctx.world, ctx.entity, condition.skill);
    else if (condition.type == AttackConditionType::Press)
        ctx.world.ecs().get<SkillPool>(ctx.entity).m_skills.at(condition.skill).m_enoughRelease = false;
    else if (condition.type == AttackConditionType::Release)
        ctx.world.ecs().get<SkillPool>(ctx.entity).m_skills.at(condition.skill).m_enoughRelease = true;
}
/// 在条件退出时完成管线事件并撤销状态守卫。
void condExit(BTCtx& ctx, const Param& param)
{
    if (std::holds_alternative<AttackCondition>(param))
    {
        const auto condition = std::get<AttackCondition>(param);
        if (condition.type == AttackConditionType::Pipe)
            skill::dealWithCastPipeEnded(ctx.world, ctx.entity, condition.skill);
        return;
    }
    const auto guard = std::get<Guard>(param);
    if (guard.status == S::Attack)
        resetSkill(ctx.world, ctx.entity);
    if (guard.status != S::Idle && guard.status != S::Revive)
        status::abortStatus(ctx.world.ecs().get<RoleStatus>(ctx.entity), guard.status);
}
/// 为行走和跑动叶子发出配置的脚步声。
void footstep(BTCtx& ctx, ActionType type)
{
    if (type != ActionType::Walk && type != ActionType::Run)
        return;
    const auto* row =
        conf::Config::instance().find<conf::RoleConfig>(ctx.world.ecs().get<RoleIdentity>(ctx.entity).roleId);
    const size_t index = type == ActionType::Walk ? 0 : 1;
    if (row && index < row->soundId.size() && row->soundId[index] != -1)
        ctx.world.emit(ctx.entity, Sound{row->soundId[index]});
}
/// 初始化普通动作叶子的动画与状态。
void enter(BTCtx& ctx, const Param& param, LeafState& state)
{
    state           = ActionState{};
    const auto type = std::get<ActionType>(param);
    behaviorStateStart(ctx.world, ctx.entity, type);
    auto& motion       = ctx.world.ecs().get<MotionState>(ctx.entity);
    motion.motionName  = animationName(type);
    motion.timeMs      = {};
    motion.m_completedLoops = 0;
    motion.loop        = true;
    motion.playing     = true;
    motion.actionScale = type == ActionType::Walk  ? conf::kWalkScaleTime
                         : type == ActionType::Run ? conf::kRunScaleTime
                                                   : conf::kIdleScaleTime;
    if (type == ActionType::Idle)
        displacement::reset(ctx.world, ctx.entity);
    footstep(ctx, type);
}
/// 推进普通动作叶子的移动和循环检测。
BTStatus tick(BTCtx& ctx, const Param& param, LeafState& state)
{
    auto& action = std::get<ActionState>(state);
    auto& ecs    = ctx.world.ecs();
    action.fTime += ecs.singleton<WorldClock>().dtMs;
    const auto type    = std::get<ActionType>(param);
    const auto& motion = ecs.get<MotionState>(ctx.entity);
    if (motion.timeMs < action.lastMotionTime)
        footstep(ctx, type);
    action.lastMotionTime = motion.timeMs;
    if (ecs.get<RoleIdentity>(ctx.entity).roleType == 1 && ecs.get<RoleHitState>(ctx.entity).m_hitProtectExecuted)
    {
        status::mixStatus(ecs.get<RoleStatus>(ctx.entity), S::HitDown);
        return BTStatus::Success;
    }
    if (type == ActionType::Idle)
        return BTStatus::Running;
    const auto command = ecs.get<RoleTree>(ctx.entity).control;
    auto& t            = ecs.get<Transform>(ctx.entity);
    auto& v            = ecs.get<RoleHitState>(ctx.entity).forceVelocity;
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
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ecs.get<RoleIdentity>(ctx.entity).roleId);
    if (row)
    {
        const Fixed speed = row->velocity * (type == ActionType::Run ? conf::kRunRate : conf::kWalkRate);
        v.x               = FixedMath::cosDeg(angle) * speed * t.vectorX;
        v.z               = FixedMath::sinDeg(angle) * speed * t.vectorZ;
    }
    return BTStatus::Running;
}
/// 通知普通动作叶子结束。
void exit(BTCtx& ctx, const Param& param, LeafState&)
{
    behaviorStateEnd(ctx.world, ctx.entity, std::get<ActionType>(param));
}
// 推挤以配置速度决定是否翻转显示朝向，移动向量仍由位移系统读取。
void jostledEnter(BTCtx& ctx, const Param&, LeafState& state)
{
    auto& ecs = ctx.world.ecs();
    const auto id = ecs.get<RoleStatus>(ctx.entity).entityParams.displacement_id;
    state = JostledState{id, false};
    behaviorStateStart(ctx.world, ctx.entity, ActionType::Jostled);
    auto& motion = ecs.get<MotionState>(ctx.entity);
    motion.motionName = animationName(ActionType::Jostled);
    motion.timeMs = {};
    motion.m_completedLoops = 0;
    motion.loop = motion.playing = true;
    motion.actionScale = conf::kWalkScaleTime;
    const auto* row = conf::Config::instance().find<conf::DisplacementConfig>(id);
    if (row && !row->velocity.empty() && row->velocity[0] != Fixed{})
        ecs.get<Transform>(ctx.entity).towardX *= -1;
    displacement::set(ctx.world, ctx.entity, id, ctx.m_node);
}
// 制动后由行为树正常退出守卫并清除推挤状态。
BTStatus jostledUpdate(BTCtx&, const Param&, LeafState& state)
{
    return std::get<JostledState>(state).m_complete ? BTStatus::Success : BTStatus::Running;
}
// 推挤结束时撤销位移监听和剩余速度。
void jostledExit(BTCtx& ctx, const Param&, LeafState&)
{
    behaviorStateEnd(ctx.world, ctx.entity, ActionType::Jostled);
    displacement::reset(ctx.world, ctx.entity);
}
// 仅首次制动翻转横向移动向量，纵向门保持方向不变。
bool jostledEvent(BTCtx& ctx, const Param&, LeafState& state)
{
    auto& action = std::get<JostledState>(state);
    if (ctx.event == DisplacementEvent::Brake && !action.m_complete)
    {
        action.m_complete = true;
        const auto* row = conf::Config::instance().find<conf::DisplacementConfig>(action.m_displacementId);
        if (row && !row->velocity.empty() && row->velocity[0] != Fixed{})
        {
            auto& transform = ctx.world.ecs().get<Transform>(ctx.entity);
            transform.vectorX = -transform.vectorX;
        }
    }
    return false;
}
// 寻路使用目标点和世界模式决定走跑，不依赖 AI 目标。
void findingEnter(BTCtx& ctx, const Param&, LeafState& state)
{
    (enter)(ctx, Param{ctx.world.setup().mode == WorldMode::Town ? ActionType::Run : ActionType::Walk}, state);
    displacement::reset(ctx.world, ctx.entity);
}
// 到达路径终点后停止速度，由守卫退出撤销 Finding 位。
BTStatus findingUpdate(BTCtx& ctx, const Param&, LeafState&)
{
    auto& ecs = ctx.world.ecs();
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ecs.get<RoleIdentity>(ctx.entity).roleId);
    if (!row) return BTStatus::Success;
    auto& transform = ecs.get<Transform>(ctx.entity);
    auto& params = ecs.get<RoleStatus>(ctx.entity).entityParams;
    const auto goal = params.m_path.empty() ? params.move_pos : params.m_path.front();
    const auto x = goal.x - transform.position.x, z = goal.z - transform.position.z;
    const auto distance = FixedMath::sqrt(x * x + z * z);
    const auto speed = row->velocity * (ctx.world.setup().mode == WorldMode::Town ? conf::kRunRate : conf::kWalkRate);
    const auto angle = FixedMath::atan2Deg(z, x);
    auto& velocity = ecs.get<RoleHitState>(ctx.entity).forceVelocity;
    velocity.x = FixedMath::cosDeg(angle) * speed;
    velocity.z = FixedMath::sinDeg(angle) * speed;
    const auto step = Fixed::ratio(1000, 60) * FixedMath::sqrt(velocity.x * velocity.x + velocity.z * velocity.z * conf::kZRate * conf::kZRate);
    if (distance <= step)
    {
        transform.position.x = goal.x; transform.position.z = goal.z;
        velocity = {};
        if (!params.m_path.empty())
        {
            params.m_path.erase(params.m_path.begin());
            if (!params.m_path.empty()) return BTStatus::Running;
        }
        auto& motion = ecs.get<MotionState>(ctx.entity);
        motion.motionName = "stand"; motion.timeMs = {}; motion.actionScale = conf::kIdleScaleTime;
        return BTStatus::Success;
    }
    if (x != Fixed{}) transform.towardX = x < Fixed{} ? -1 : 1;
    transform.vectorX = Fixed::fromInt(transform.towardX);
    transform.vectorZ = z < Fixed{} ? "-1"_fx : "1"_fx;
    return BTStatus::Running;
}
// 寻路退出时配对走跑行为通知并清除剩余位移。
void findingExit(BTCtx& ctx, const Param&, LeafState& state)
{
    (exit)(ctx, Param{ctx.world.setup().mode == WorldMode::Town ? ActionType::Run : ActionType::Walk}, state);
    displacement::reset(ctx.world, ctx.entity);
}
/// 未实现叶子的空进入回调。
void unimplementedEnter(BTCtx&, const Param&, LeafState&) {}
/// 未实现叶子保持运行，避免擅自改变树状态。
BTStatus unimplementedUpdate(BTCtx&, const Param&, LeafState&)
{
    return BTStatus::Running;
}
/// 未实现叶子的空退出回调。
void unimplementedExit(BTCtx&, const Param&, LeafState&) {}
/// 为节点追加一个条件并记录其参数索引。
void addCondition(std::vector<BTCondDef>& conditions, std::vector<Param>& params, BTNodeDef& node, Param param)
{
    node.firstCond = static_cast<uint32_t>(conditions.size());
    node.condCount = 1;
    conditions.push_back({0, static_cast<uint32_t>(params.size())});
    params.push_back(std::move(param));
}
/// 为一个方向动作列构建顺序攻击叶子。
void addToward(std::vector<BTNodeDef>& nodes,
               std::vector<BTCondDef>& conditions,
               std::vector<Param>& params,
               int32_t skill,
               int32_t toward,
               const std::vector<int32_t>& actions)
{
    if (actions.empty() || actions.front() == -1)
        return;
    BTNodeDef node;
    node.type       = BTNodeType::Sequence;
    node.childCount = static_cast<uint16_t>(actions.size());
    addCondition(conditions, params, node, AttackCondition{AttackConditionType::VectorIndex, skill, 1, toward});
    nodes.push_back(node);
    for (size_t i = 0; i < actions.size(); ++i)
    {
        BTNodeDef leaf;
        leaf.leafTypeId = kAttackLeaf;
        leaf.paramIndex = static_cast<uint32_t>(params.size());
        params.emplace_back(AttackAction{actions[i], static_cast<int32_t>(i + 1), skill});
        nodes.push_back(leaf);
    }
}
/// 为按住与松开两种释放方式构建选择分支。
void addReleaseType(std::vector<BTNodeDef>& nodes,
                    std::vector<BTCondDef>& conditions,
                    std::vector<Param>& params,
                    int32_t skill,
                    const conf::SkillAttackConfig& row)
{
    const uint32_t selector = static_cast<uint32_t>(nodes.size());
    nodes.push_back(BTNodeDef{BTNodeType::Selector});
    uint16_t releaseTypes = 0;
    for (const auto type : {AttackConditionType::Release, AttackConditionType::Press})
    {
        const auto& actions = type == AttackConditionType::Release ? row.upActionIds : row.actionIds;
        uint16_t count      = 0;
        for (const auto& action : actions)
            if (!action.empty() && action.front() != -1)
                ++count;
        if (count == 0)
            continue;
        BTNodeDef node;
        node.type       = BTNodeType::Sequence;
        node.childCount = count;
        addCondition(conditions, params, node, AttackCondition{type, skill});
        nodes.push_back(node);
        ++releaseTypes;
        for (size_t i = 0; i < actions.size(); ++i)
            addToward(nodes, conditions, params, skill, static_cast<int32_t>(i + 1), actions[i]);
    }
    /// 仅计入实际追加的释放分支，保持子节点拓扑连续。
    nodes[selector].childCount = releaseTypes;
}
/// 按可用次数从高到低构建技能管线分支。
void addPipe(std::vector<BTNodeDef>& nodes,
             std::vector<BTCondDef>& conditions,
             std::vector<Param>& params,
             int32_t skill,
             const SkillTreeSource& source,
             const conf::SkillAttackConfig& row)
{
    for (int32_t pipe = source.releaseMax; pipe >= 1; --pipe)
    {
        BTNodeDef node;
        node.type       = BTNodeType::Selector;
        node.childCount = row.skillType == 1 ? 1
                                             : static_cast<uint16_t>(std::count_if(
                                                   row.actionIds.begin(), row.actionIds.end(), [](const auto& actions) {
            return !actions.empty() && actions.front() != -1;
        }));
        addCondition(conditions, params, node, AttackCondition{AttackConditionType::Pipe, skill, pipe});
        nodes.push_back(node);
        if (row.skillType == 1)
            addReleaseType(nodes, conditions, params, skill, row);
        else
            for (size_t i = 0; i < row.actionIds.size(); ++i)
                addToward(nodes, conditions, params, skill, static_cast<int32_t>(i + 1), row.actionIds[i]);
    }
}
/// 将同槽位技能组织为攻击选择子树。
void addAttackTree(std::vector<BTNodeDef>& nodes,
                   std::vector<BTCondDef>& conditions,
                   std::vector<Param>& params,
                   const DefinitionKey& key,
                   const conf::Config& config)
{
    for (size_t begin = 0; begin < key.skills.size();)
    {
        size_t end = begin;
        while (end < key.skills.size() && key.skills[end].slot == key.skills[begin].slot)
            ++end;
        BTNodeDef slotNode;
        slotNode.type  = BTNodeType::Selector;
        uint16_t steps = 0;
        for (size_t index = begin; index < end; ++index)
            if (config.find<conf::SkillAttackConfig>(key.skills[index].configId))
                ++steps;
        if (steps == 0)
        {
            begin = end;
            continue;
        }
        slotNode.childCount = steps;
        addCondition(conditions, params, slotNode,
                     AttackCondition{AttackConditionType::Slot, static_cast<int32_t>(begin)});
        nodes.push_back(slotNode);
        for (size_t index = begin; index < end; ++index)
        {
            const auto& source = key.skills[index];
            const auto* row    = config.find<conf::SkillAttackConfig>(source.configId);
            if (!row)
                continue;
            BTNodeDef node;
            node.type       = BTNodeType::Selector;
            node.childCount = static_cast<uint16_t>(source.releaseMax);
            addCondition(conditions, params, node,
                         AttackCondition{AttackConditionType::SkillStep, static_cast<int32_t>(index)});
            nodes.push_back(node);
            addPipe(nodes, conditions, params, static_cast<int32_t>(index), source, *row);
        }
        begin = end;
    }
}
/// 根据模板和已加载技能构建不可变 DFS 行为树拓扑。
Def build(const DefinitionKey& key, const conf::Config& config)
{
    constexpr std::array statuses{S::Death,     S::Revive, S::Wake,   S::GetUp,   S::HitFloor,  S::HitDown, S::HitUp,
                                  S::HitSwitch, S::Hit,    S::Attack, S::Jostled, S::Patrol,    S::Chase,   S::Alert,
                                  S::Finding,   S::Run,    S::Walk,   S::Idle,    S::Controlled};
    constexpr std::array actions{
        ActionType::Death,   ActionType::Revive, ActionType::Wake,      ActionType::GetUp,     ActionType::HitFloor,
        ActionType::HitDown, ActionType::HitUp,  ActionType::HitSwitch, ActionType::Hit,       ActionType::None,
        ActionType::Jostled, ActionType::Patrol, ActionType::Chase,     ActionType::Alert,     ActionType::Move,
        ActionType::Run,     ActionType::Walk,   ActionType::Idle,      ActionType::Controlled};
    const bool city = key.tree == Template::City;
    std::vector<BTNodeDef> nodes;
    std::vector<BTCondDef> conditions;
    std::vector<Param> params{std::monostate{}};
    BTNodeDef root;
    root.type       = BTNodeType::Selector;
    root.childCount = city ? 2 : 19;
    nodes.push_back(root);
    for (uint16_t i = 0; i < root.childCount; ++i)
    {
        const S status          = city ? (i == 0 ? S::Finding : S::Idle) : statuses[i];
        const ActionType action = city ? (i == 0 ? ActionType::Move : ActionType::Idle) : actions[i];
        BTNodeDef branch;
        branch.type       = status == S::Death    ? BTNodeType::Sequence
                            : status == S::Attack ? BTNodeType::Selector
                                                  : BTNodeType::Parallel;
        branch.childCount = status == S::Attack  ? static_cast<uint16_t>(key.skills.empty() ? 0 : 1)
                            : status == S::Death ? 2
                                                 : 1;
        addCondition(conditions, params, branch, Guard{status, city});
        nodes.push_back(branch);
        if (status == S::Attack)
        {
            if (!key.skills.empty())
            {
                uint16_t slots = 0;
                for (size_t index = 0; index < key.skills.size();)
                {
                    const auto slot = key.skills[index].slot;
                    bool valid      = false;
                    while (index < key.skills.size() && key.skills[index].slot == slot)
                    {
                        valid = valid || config.find<conf::SkillAttackConfig>(key.skills[index].configId);
                        ++index;
                    }
                    if (valid)
                        ++slots;
                }
                nodes.back().childCount = slots;
                addAttackTree(nodes, conditions, params, key, config);
            }
            continue;
        }
        for (uint16_t j = 0; j < branch.childCount; ++j)
        {
            BTNodeDef leaf;
            leaf.leafTypeId = action == ActionType::Idle   ? kIdleLeaf
                              : action == ActionType::Walk ? kWalkLeaf
                              : action == ActionType::Run  ? kRunLeaf
                              : action == ActionType::Jostled ? kJostledLeaf
                              : action == ActionType::Patrol || action == ActionType::Chase || action == ActionType::Alert ? kAiMoveLeaf
                              : !city && (i <= 8 || status == S::Controlled) ? kHitStateLeaf
                                                           : kFindingLeaf;
            leaf.paramIndex = static_cast<uint32_t>(params.size());
            params.emplace_back(j == 0 ? action : ActionType::Destroy);
            nodes.push_back(leaf);
        }
    }
    return Def(std::move(nodes), std::move(conditions), std::move(params));
}
}  // namespace
/// 查表取得普通动作的动画名称。
const char* animationName(ActionType type)
{
    constexpr std::array names{"",        "death",   "getup",   "toground", "flydown",  "flyup", "hitswitch",
                               "hit",     "flydown", "running", "running",  "running",  "walk",  "running",
                               "stand",   "spurt",   "death",   "active",   "transfer", "walk",  "running",
                               "running", "running", "running", "special",  "hit", "stand", "stand", ""};
    return names.at(static_cast<size_t>(type));
}
// 生命周期匹配使用动作类名，不使用可共享的动画名。
const char* behaviorName(ActionType type)
{
    constexpr std::array names{"ActionBase", "ActionDeath", "ActionGetUp", "ActionHitFloor", "ActionHitDown",
        "ActionHitUp", "ActionHitSwitch", "ActionHit", "ActionFall", "ActionPatrol", "ActionChase", "ActionAlert",
        "ActionRun", "ActionWalk", "ActionIdle", "ActionSpurt", "ActionCollect", "ActionActive", "ActionTransfer",
        "ActionMove", "ActionReadyIdle", "ActionReadyMove", "ActionJostled", "ActionFollow", "ActionSpecIdle",
        "ActionControlled", "ActionWake", "ActionRevive", "ActionDestroy"};
    return names.at(static_cast<size_t>(type));
}
// 将普通动作开始通知转交给世界拥有的 Buff 接缝。
void behaviorStateStart(World& world, Entity entity, ActionType type)
{
    if (world.hitHooks().m_behaviorState)
        world.hitHooks().m_behaviorState(world, entity, BFEvent::BehaviorStateStart, behaviorName(type));
}
/// 将普通动作结束通知转交给世界拥有的 Buff 接缝。
void behaviorStateEnd(World& world, Entity entity, ActionType type)
{
    if (world.hitHooks().m_behaviorState)
        world.hitHooks().m_behaviorState(world, entity, BFEvent::BehaviorStateEnd, behaviorName(type));
}
/// 收集实体技能装配以生成定义缓存键。
DefinitionKey definitionKey(World& world, Entity entity)
{
    DefinitionKey key;
    key.tree         = world.setup().mode == WorldMode::Town ? Template::City : Template::Role;
    const auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (!pool)
        return key;
    key.skills.reserve(pool->m_skills.size());
    for (const auto& skill : pool->m_skills)
        key.skills.push_back({skill.m_address.m_slot, skill.m_address.m_index, skill.m_address.m_mode,
                              skill.m_address.m_step, skill.m_configId, skill.m_releaseMax});
    return key;
}
/// 从只读缓存获取或构建行为树定义。
const Def& definition(const DefinitionKey& key)
{
    static BTDefCache<DefinitionKey, Param, conf::Config> cache(conf::Config::instance(), build);
    return cache.get(key);
}
/// 返回共享且只读的角色行为树回调表。
const Registry& registry()
{
    static const Registry value({{unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {enter, tick, exit},
                                 {enter, tick, exit},
                                 {enter, tick, exit},
                                 {attack::enter, attack::tick, attack::exit, attack::onDisplacementEvent},
                                 {hit_actions::enter, hit_actions::tick, hit_actions::exit, hit_actions::onDisplacementEvent},
                                 {ai::movement::enter, ai::movement::tick, ai::movement::exit, ai::movement::onDisplacementEvent},
                                 {jostledEnter, jostledUpdate, jostledExit, jostledEvent},
                                 {findingEnter, findingUpdate, findingExit}},
                                {{check, condEnter, condExit}});
    return value;
}
/// 首次创建实例后执行一帧行为树。
void update(World& world, Entity entity)
{
    auto& tree      = world.ecs().get<RoleTree>(entity);
    const auto& def = definition(tree.defKey);
    BTCtx ctx{world, entity};
    if (tree.instance.nodes.empty())
    {
        tree.instance = BTInstance<LeafState>(def);
        BTRunner<BTCtx, Param, LeafState>(def, registry(), tree.instance).enter(ctx);
    }
    BTRunner<BTCtx, Param, LeafState>(def, registry(), tree.instance).update(ctx);
}
/// 依次处理本帧控制队列并清空队列。
void consumeControl(World& world, Entity entity)
{
    auto& ecs   = world.ecs();
    auto& queue = ecs.get<ControlEventQueue>(entity).commands;
    for (const auto& command : queue)
    {
        if (!ecs.get<ExtraState>(entity).isCanControl() || status::isJostledStatus(ecs.get<RoleStatus>(entity)))
            continue;
        ecs.get<RoleTree>(entity).control = command;
        if (static_cast<int32_t>(command.controlEvent) >= static_cast<int32_t>(ControlEvent::CTR_ATKA))
            skillEvent(world, entity, command);
        else if (command.controlEvent == ControlEvent::CTR_IDLE)
            status::dealWithStatus(world, entity, S::Idle);
        else if (command.controlEvent == ControlEvent::CTR_RUN)
        {
            skill::interruptRun(world, entity);
            status::dealWithStatus(world, entity, S::Run);
        }
        else if (command.controlEvent == ControlEvent::CTR_WALK)
            status::dealWithStatus(world, entity, S::Walk);
    }
    queue.clear();
}
/// 将位移事件精确派发给登记的监听节点。
bool onDisplacementEvent(World& world, Entity entity, DisplacementEvent event)
{
    const int32_t nodeId = world.ecs().get<Displacement>(entity).listenerNode;
    if (nodeId < 0)
        return false;
    auto& tree          = world.ecs().get<RoleTree>(entity);
    const auto& def     = definition(tree.defKey);
    const auto& node    = def.topology().node(static_cast<uint32_t>(nodeId));
    const auto callback = registry().leaf(node.leafTypeId).onDisplacementEvent;
    if (!callback)
        return false;
    BTCtx ctx{world, entity, event};
    ctx.m_node = nodeId;
    return callback(ctx, def.param(node.paramIndex),
                    tree.instance.leaves[def.topology().leafIndex(static_cast<uint32_t>(nodeId))]);
}
}  // namespace mugen::sim::behavior
