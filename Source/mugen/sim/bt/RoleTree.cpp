#include "RoleTree.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/core/math/FixedMath.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/attack/AttackRole.h"
#include "mugen/sim/role/brain/RoleContext.h"

namespace mugen::sim::behavior
{
namespace
{
using S = EntityRoleStatus;
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
/// 构建仅含攻击子树的定义，供新状态机桥接 B 复用，不复制构建逻辑。
Def buildAttackOnly(const DefinitionKey& key, const conf::Config& config)
{
    std::vector<BTNodeDef> nodes;
    std::vector<BTCondDef> conditions;
    std::vector<Param> params{std::monostate{}};
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
    BTNodeDef root;
    root.type       = BTNodeType::Selector;
    root.childCount = slots;
    nodes.push_back(root);
    addAttackTree(nodes, conditions, params, key, config);
    return Def(std::move(nodes), std::move(conditions), std::move(params));
}
/// 按缓存键重建攻击子树定义（无进程级缓存，桥接 B 每次调用重建）。
Def attackDefinition(const DefinitionKey& key)
{
    return buildAttackOnly(key, conf::Config::instance());
}
// 临时桥接，阶段 3 删除：为攻击子树构造攻击叶回调。
BTLeafCallbacks<BTCtx, Param, LeafState> attackLeafCallbacks()
{
    BTLeafCallbacks<BTCtx, Param, LeafState> result;
    result.onEnter = [](BTCtx& ctx, const Param& param, LeafState& state) {
        if (!std::holds_alternative<attack::AttackRole>(state))
            state = attack::AttackRole{};
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        attack::AttackAction::enter(rc, std::get<AttackAction>(param), std::get<attack::AttackRole>(state));
    };
    result.onUpdate = [](BTCtx& ctx, const Param& param, LeafState& state) {
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        return attack::AttackAction::update(rc, std::get<AttackAction>(param), std::get<attack::AttackRole>(state));
    };
    result.onExit = [](BTCtx& ctx, const Param& param, LeafState& state) {
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        attack::AttackAction::exit(rc, std::get<AttackAction>(param), std::get<attack::AttackRole>(state));
    };
    result.onDisplacementEvent = [](BTCtx& ctx, const Param& param, LeafState& state) {
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        rc.setEvent(ctx.event);
        return attack::AttackAction::onDisplacement(rc, std::get<AttackAction>(param),
                                                    std::get<attack::AttackRole>(state));
    };
    return result;
}
/// 返回共享且只读的回调表（桥接 B 阶段仅攻击子树使用 kAttackLeaf）。
const Registry& registry()
{
    static const Registry value({{unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 attackLeafCallbacks(),
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit},
                                 {unimplementedEnter, unimplementedUpdate, unimplementedExit}},
                                {{check, condEnter, condExit}});
    return value;
}
}  // namespace mugen::sim::behavior
