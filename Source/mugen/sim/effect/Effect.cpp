#include "Effect.h"
#include "mugen/sim/World.h"
#include "mugen/sim/attack/ActionAttack.h"
#include "mugen/sim/status/Status.h"
#include "mugen/sim/skill/SkillTriggers.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/avatar/MotionPlayer.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/math/FixedMath.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/trace/TraceSink.h"
#include <charconv>

namespace mugen::sim::spawn
{
// 从效果位置召唤，出生前确定主人、阵营和继承属性。
void summon(World& world, Entity effect, int32_t roleId, int64_t lifetime)
{
    auto& ecs = world.ecs();
    const auto owner = ecs.get<EffectState>(effect).m_owner;
    const auto* identity = ecs.tryGet<RoleIdentity>(owner);
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(roleId);
    if (!identity || !row) return;
    const auto source = *identity;
    const auto transform = ecs.get<Transform>(effect);
    RoleSpawnParams params;
    params.roleId = roleId;
    params.camp = source.camp;
    params.level = source.level;
    params.aiDifficulty = source.aiDifficulty;
    params.isAi = true;
    params.position = transform.position;
    params.toward = transform.towardX;
    if (row->roleType == static_cast<int32_t>(EntityRoleType::kSummon)) params.m_owner = owner;
    const auto& levels = ecs.singleton<RoomState>().m_config.monsterLevels;
    if (row->roleType >= 2 && row->roleType <= 4 && static_cast<size_t>(row->roleType - 2) < levels.size())
        params.level = levels[row->roleType - 2] + ecs.singleton<RoomState>().m_levelBonus;
    const auto entity = spawnRole(world, params);
    if (entity.value == 0) return;
    if (const auto* drops = ecs.tryGet<DropGoods>(owner)) ecs.add(entity, *drops);
    ecs.get<RoleTimers>(entity).summonMs = Fixed::fromInt(lifetime);
    ecs.get<Transform>(entity).vectorX = transform.vectorX;
    ecs.get<Transform>(entity).vectorZ = transform.vectorZ;
}
}

namespace mugen::sim::effect
{
namespace
{
/// 特效树瞬时上下文；实例在栈上运行，子实体插入不会使其引用失效。
struct Context
{
    /// 所属世界。
    World& m_world;
    /// 当前特效。
    Entity m_entity;
    /// 本次运行的树实例。
    EffectTree& m_tree;
    /// 由执行器写入的当前节点标识。
    int32_t m_node = -1;
};
/// 特效树节点参数为动作配置标识。
using Def = BTDef<int32_t>;
/// 特效专用回调注册表。
using Registry = BTRegistry<Context, int32_t, std::variant<AttackEffect>>;
/// 将字符串形态的整数配置解析为数值，失败时记录配置问题。
int32_t integer(const std::string& text)
{
    int32_t value = -1;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    {
        MG_LOG_W("Invalid integer effect configuration: {}", text);
        return -1;
    }
    return value;
}
/// 将特效动作参数投影到共用动作算法，不另存配置副本。
conf::ActionConfig common(const conf::ActionEffectConfig& row)
{
    conf::ActionConfig result;
    result.loop = Fixed::fromInt(row.loop);
    result.actionDelayTime = static_cast<int32_t>(std::clamp(row.actionDelayTime, int64_t{0}, int64_t{INT32_MAX}));
    result.displacementId = row.displacementId;
    result.obstruct = row.obstruct;
    result.floor = row.floor;
    result.cameraId = row.cameraId;
    result.cameraFrame = row.cameraFrame;
    return result;
}
/// 特效攻击只由统一状态位控制。
bool check(Context& ctx, const int32_t&)
{
    return ctx.m_world.ecs().get<RoleStatus>(ctx.m_entity).isAttack();
}
/// 条件进入不产生额外状态源。
void conditionEnter(Context&, const int32_t&) {}
/// 条件退出清除统一攻击状态。
void conditionExit(Context& ctx, const int32_t&)
{
    ctx.m_world.ecs().get<RoleStatus>(ctx.m_entity).abort(EntityRoleStatus::Attack);
}
/// 选择目标音效分组；缺失或单个 -1 时退回默认组。
std::vector<int32_t> sounds(World& world, Entity entity, const conf::ActionEffectConfig& row)
{
    const auto target = world.ecs().get<EffectState>(entity).m_hitEntity;
    const auto* identity = world.ecs().tryGet<RoleIdentity>(target);
    const auto* role = identity ? conf::Config::instance().find<conf::RoleConfig>(identity->roleId) : nullptr;
    if (!role || role->soundType <= 0) return row.soundId;
    std::vector<int32_t> result;
    const size_t index = static_cast<size_t>(role->soundType - 1);
    std::visit([&](const auto& groups) {
        if (index >= groups.size()) return;
        for (const auto& value : groups[index])
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>)
                result.push_back(integer(value));
            else result.push_back(value);
        }
    }, row.soundId2);
    return result.empty() || (result.size() == 1 && result[0] == -1) ? row.soundId : result;
}
/// 读取施法者输入，特效没有第二份输入队列。
PlayerCommand control(World& world, Entity entity)
{
    const auto owner = world.ecs().get<EffectState>(entity).m_owner;
    const auto* control = world.ecs().tryGet<RoleControl>(owner);
    return control ? control->m_last : PlayerCommand{};
}
/// 将施法者摇杆方向应用于特效。
void direction(World& world, Entity entity, bool facing)
{
    const auto command = control(world, entity);
    auto& transform = world.ecs().get<Transform>(entity);
    if (command.quadrant == 0)
        return;
    if (command.angle != 90000)
    {
        transform.vectorX = command.quadrant <= 2 ? "1"_fx : "-1"_fx;
        if (facing)
            transform.towardX = static_cast<int32_t>(transform.vectorX.toInt());
    }
    if (command.angle != 0)
        transform.vectorZ = command.quadrant == 1 || command.quadrant == 4 ? "1"_fx : "-1"_fx;
}
/// 按共用动作控制规则更新特效速度。
void moveControl(World& world, Entity entity, const conf::ActionEffectConfig& row)
{
    const auto type = std::holds_alternative<int32_t>(row.control) ? std::get<int32_t>(row.control) : integer(std::get<std::string>(row.control));
    if (type == 2 && row.displacementId != -1) direction(world, entity, true);
    else if ((type == 0 || type == 3) && row.displacementId == -1)
    {
        direction(world, entity, type == 0);
        const auto command = control(world, entity);
        const auto transform = world.ecs().get<Transform>(entity);
        auto& velocity = world.ecs().get<RoleHitState>(entity).forceVelocity;
        const auto angle = Fixed::ratio(command.angle, 1000);
        velocity.x = command.quadrant == 0 ? Fixed{} : FixedMath::cosDeg(angle) * row.controlVelocity * transform.vectorX;
        velocity.z = command.quadrant == 0 ? Fixed{} : FixedMath::sinDeg(angle) * row.controlVelocity * transform.vectorZ;
    }
}
/// 初始化单段动画、飞行位移及声音。
void actionEnter(Context& ctx, const int32_t& id, std::variant<AttackEffect>& value)
{
    auto& state = std::get<AttackEffect>(value);
    state = {};
    const auto* row = conf::Config::instance().find<conf::ActionEffectConfig>(id);
    if (!row) { state.m_action.m_finished = true; return; }
    auto& world = ctx.m_world;
    auto& ecs = world.ecs();
    const auto entity = ctx.m_entity;
    const auto* scale = std::get_if<Fixed>(&row->actionScaleTime);
    if (!scale || *scale <= Fixed{})
    {
        MG_LOG_E("Invalid action scale for effect action {}", id);
        state.m_action.m_finished = true;
        return;
    }
    const auto owner = ecs.get<EffectState>(entity).m_owner;
    const auto* ownerHit = ecs.tryGet<RoleHitState>(owner);
    const auto rate = *scale * (ownerHit ? ownerHit->m_actionScale : "1"_fx);
    if (rate <= Fixed{})
    {
        MG_LOG_E("Effect action scale must be positive: {}", id);
        MG_ASSERT(false && "Effect action scale must be positive");
        state.m_action.m_finished = true;
        return;
    }
    if (std::holds_alternative<int32_t>(row->control) && std::get<int32_t>(row->control) == 1)
        direction(world, entity, true);
    auto& motion = ecs.get<MotionState>(entity);
    motion.motionName = std::holds_alternative<std::string>(row->action) ? std::get<std::string>(row->action) : std::to_string(std::get<int32_t>(row->action));
    motion.entryId.clear();
    motion.actionScale = rate;
    motion.timeMs = {};
    motion.m_completedLoops = 0;
    motion.loop = row->loop == -1 || row->loop > 1;
    motion.playing = true;
    auto& action = state.m_action;
    action.m_interval = "1000"_fx * kLogicDtSec / rate;
    action.m_effects.resize(row->effectIds.size());
    action.m_spawned.resize(row->effectIds.size());
    action.m_shaken.resize(row->cameraFrame.size());
    int32_t node = -1;
    for (size_t i = 0; i < ctx.m_tree.m_instance.leaves.size(); ++i)
        if (&ctx.m_tree.m_instance.leaves[i] == &value) node = static_cast<int32_t>(i + 1);
    displacement::reset(world, entity);
    displacement::set(world, entity, row->displacementId, node);
    if (row->ghost != -1) world.emit(entity, Ghost{"-1"_fx});
    world.emit(entity, Sound{0, sounds(world, entity, *row), false, id});
}
/// 更新单段计时、严格帧阈值子效果与召唤接缝。
BTStatus actionTick(Context& ctx, const int32_t& id, std::variant<AttackEffect>& value)
{
    auto& state = std::get<AttackEffect>(value);
    const auto* row = conf::Config::instance().find<conf::ActionEffectConfig>(id);
    if (!row || state.m_action.m_finished) return BTStatus::Success;
    auto& world = ctx.m_world;
    const auto entity = ctx.m_entity;
    auto& action = state.m_action;
    action.m_time += world.ecs().singleton<WorldClock>().dtMs;
    const auto shared = common(*row);
    attack::dealWithTime(action, shared);
    moveControl(world, entity, *row);
    for (size_t i = 0; i < row->effectIds.size() && i < row->effectFrames.size(); ++i)
        if (!action.m_spawned[i] && row->effectIds[i] != -1 && action.m_time > action.m_interval * Fixed::fromInt(row->effectFrames[i]))
        {
            action.m_spawned[i] = 1;
            action.m_effects[i] = spawnChild(world, entity, row->effectIds[i]);
        }
    attack::dealWithShake(world, entity, action, shared);
    if (!state.m_summoned && row->summonId != -1 && action.m_time >= action.m_interval * Fixed::fromInt(row->summonFrame))
    {
        state.m_summoned = true;
        const auto owner = world.ecs().get<EffectState>(entity).m_owner;
        const auto* status = world.ecs().tryGet<RoleStatus>(owner);
        if (status && !status->isDeath())
            spawn::summon(world, entity, row->summonId, row->summonTime);
    }
    return action.m_finished ? BTStatus::Success : BTStatus::Running;
}
/// 动作退出仅清理本段自动释放子效果。
void actionExit(Context& ctx, const int32_t& id, std::variant<AttackEffect>& value)
{
    auto& state = std::get<AttackEffect>(value);
    attack::clearEffects(ctx.m_world, ctx.m_entity, state.m_action);
    ctx.m_world.ecs().get<MotionState>(ctx.m_entity).actionScale = "1"_fx;
    displacement::reset(ctx.m_world, ctx.m_entity);
    const auto* row = conf::Config::instance().find<conf::ActionEffectConfig>(id);
    if (row && row->ghost != -1) ctx.m_world.emit(ctx.m_entity, Ghost{});
    ctx.m_world.emit(ctx.m_entity, Sound{0, {}, true, id});
}
/// 最后一个叶子进入时请求销毁。
void destroyEnter(Context& ctx, const int32_t&, std::variant<AttackEffect>&) { ctx.m_world.destroy(ctx.m_entity); }
/// 销毁叶子完成。
BTStatus destroyTick(Context&, const int32_t&, std::variant<AttackEffect>&) { return BTStatus::Success; }
/// 销毁叶子不持有动作资源。
void destroyExit(Context&, const int32_t&, std::variant<AttackEffect>&) {}
/// 构造带攻击守卫的顺序树，最后追加销毁叶子。
Def definition(const EffectTree& tree)
{
    std::vector<BTNodeDef> nodes;
    BTNodeDef root;
    root.type = BTNodeType::Sequence;
    root.childCount = static_cast<uint16_t>(tree.m_actions.size() + 1);
    root.condCount = 1;
    nodes.push_back(root);
    std::vector<int32_t> params{0};
    for (const auto id : tree.m_actions)
    {
        BTNodeDef leaf;
        leaf.paramIndex = static_cast<uint32_t>(params.size());
        params.push_back(id);
        nodes.push_back(leaf);
    }
    BTNodeDef destroy;
    destroy.leafTypeId = 1;
    nodes.push_back(destroy);
    return Def(std::move(nodes), {{0, 0}}, std::move(params));
}
/// 只读函数表可由所有世界共享。
const Registry& registry()
{
    static const Registry value({{actionEnter, actionTick, actionExit}, {destroyEnter, destroyTick, destroyExit}},
                                {{check, conditionEnter, conditionExit}});
    return value;
}
/// 将动画推进和碰撞框采样交给与角色共用的运动入口。
void motionUpdate(World& world, Entity entity) { role::updateMotion(world, entity); }
}

/// 创建基础组件；属性始终通过所属实体读取。
Entity create(World& world, Entity owner, int32_t id)
{
    const auto* row = conf::Config::instance().find<conf::EffectConfig>(id);
    if (!row) { MG_LOG_W("Missing effect configuration {}", id); return {}; }
    EffectState state;
    state.m_owner = owner;
    state.m_configId = id;
    EffectTree tree;
    for (const auto action : row->actionIds) if (action != -1) tree.m_actions.push_back(action);
    MotionState motion;
    Transform transform;
    if (const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(row->resSpineId))
    {
        motion.motionFile = AvatarPaths::motionFileFromSpine(spine->fileName);
        transform.scale = spine->scale;
    }
    auto& ecs = world.ecs();
    const auto entity = world.create(GroupKind::Effect);
    ecs.add(entity, state);
    ecs.add(entity, tree);
    ecs.add(entity, motion);
    ecs.add(entity, transform);
    ecs.add(entity, RoleStatus{EntityRoleStatus::Attack, {}});
    ecs.add(entity, RoleHitState{});
    ecs.add(entity, ExtraState{});
    ecs.add(entity, Displacement{});
    RoleIdentity identity;
    identity.roleId = 0;
    identity.roleType = 0;
    identity.camp = 1;
    identity.hostileCamp = 30;
    ecs.add(entity, identity);
    ecs.add(entity, ColliderState{{}, {}, ecs.singleton<WorldClock>().frame, Fixed::fromInt(row->radius)});
    ecs.add(entity, CollisionState{});
    trace::noteSpawn(world, owner, id);
    return entity;
}
/// 应用地图相对、地图绝对或实体相对坐标。
void setCustomPosition(World& world, Entity entity, Entity base, bool ground)
{
    auto& ecs = world.ecs();
    const auto source = ecs.get<Transform>(base);
    const auto& row = conf::Config::instance().get<conf::EffectConfig>(ecs.get<EffectState>(entity).m_configId);
    auto& target = ecs.get<Transform>(entity);
    target.towardX = row.effectOriebtationX == 0 ? -1 : row.effectOriebtationX == 1 ? 1 : source.towardX;
    target.vectorX = row.effectOriebtationX == 0 ? "-1"_fx : row.effectOriebtationX == 1 ? "1"_fx : source.vectorX;
    target.vectorZ = row.effectOriebtationZ == 1 ? "1"_fx : source.vectorZ;
    const auto axis = [&](size_t i) { return i < row.relativePosition.size() ? row.relativePosition[i] : Fixed{}; };
    FixedVec3 position{axis(0), axis(1), axis(2)};
    const auto& ranges = ecs.singleton<MapRegion>().moveRanges;
    if (row.positionType == 0 && !ranges.empty())
    {
        position.x = ranges.front().x + ranges.front().width * position.x;
        position.z = ranges.front().y + ranges.front().height * position.z;
    }
    if (row.positionType == 0 || row.positionType == 1) position.x += Fixed::fromInt(target.towardX);
    else
    {
        position.x = source.position.x + Fixed::fromInt(target.towardX) * position.x;
        position.y += source.position.y;
        position.z += source.position.z - "1"_fx;
    }
    if (ground) position.y = {};
    target.position = position;
}
/// 归属角色消失时保留上次位置，不将独立特效提前销毁。
void follow(World& world, Entity entity)
{
    const auto state = world.ecs().get<EffectState>(entity);
    const auto& row = conf::Config::instance().get<conf::EffectConfig>(state.m_configId);
    if ((row.follow == 1 || row.follow == 2) && world.ecs().has<EntityGroup>(state.m_owner) && world.ecs().has<Transform>(state.m_owner))
        setCustomPosition(world, entity, state.m_owner, row.follow == 2);
}
/// 在栈上运行树实例，避免子效果扩容当前组件池时留下悬空引用。
void enter(World& world, Entity entity)
{
    auto tree = world.ecs().get<EffectTree>(entity);
    const auto def = definition(tree);
    tree.m_instance = BTInstance<std::variant<AttackEffect>>(def);
    Context ctx{world, entity, tree};
    BTRunner<Context, int32_t, std::variant<AttackEffect>>(def, registry(), tree.m_instance).enter(ctx);
    world.ecs().get<EffectTree>(entity) = std::move(tree);
}
/// 子效果保持最初施法者和技能信息，位置以父效果为基准。
Entity spawnChild(World& world, Entity parent, int32_t id)
{
    const auto source = world.ecs().get<EffectState>(parent);
    const auto entity = create(world, source.m_owner, id);
    if (entity.value == 0) return {};
    auto& state = world.ecs().get<EffectState>(entity);
    state.m_skillId = source.m_skillId;
    state.m_baseSkillId = source.m_baseSkillId;
    state.m_slot = source.m_slot;
    state.m_skillAttributeTag = source.m_skillAttributeTag;
    state.m_artifactId = source.m_artifactId;
    state.m_artifactIndex = source.m_artifactIndex;
    state.m_capabilityTrigger = source.m_capabilityTrigger;
    setCustomPosition(world, entity, parent);
    follow(world, entity);
    enter(world, entity);
    return entity;
}
/// 更新顺序保留先组件、后冻结、再生命周期的帧边界。
void update(World& world, Entity entity)
{
    auto& ecs = world.ecs();
    if (!ecs.has<EffectState>(entity)) return;
    const auto id = ecs.get<EffectState>(entity).m_configId;
    const auto& row = conf::Config::instance().get<conf::EffectConfig>(id);
    const auto* hit = conf::Config::instance().find<conf::SkillHitConfig>(row.hitId);
    if (ecs.get<EffectState>(entity).m_hitCount != 0)
    {
        ecs.get<EffectState>(entity).m_hitCount = 0;
        if (hit) ecs.get<EffectState>(entity).m_hitInterval = Fixed::fromInt(hit->hitInterval);
    }
    if (!ecs.get<EffectState>(entity).m_frozen && !ecs.get<EntityGroup>(entity).destroyRequested)
    {
        auto tree = ecs.get<EffectTree>(entity);
        const auto def = definition(tree);
        Context ctx{world, entity, tree};
        BTRunner<Context, int32_t, std::variant<AttackEffect>>(def, registry(), tree.m_instance).update(ctx);
        ecs.get<EffectTree>(entity) = std::move(tree);
        if (!ecs.get<EntityGroup>(entity).destroyRequested)
        {
            displacement::update(world, entity);
            motionUpdate(world, entity);
            if (!hit) { ecs.get<ColliderState>(entity).attack.clear(); ecs.get<ColliderState>(entity).damage.clear(); }
        }
    }
    auto& state = ecs.get<EffectState>(entity);
    const auto dt = ecs.singleton<WorldClock>().dtMs;
    state.m_freezeMs = std::max(Fixed{}, state.m_freezeMs - dt);
    state.m_frozen = state.m_freezeMs > Fixed{};
    if (state.m_frozen) return;
    if ((row.autoRelease == 2 || row.autoRelease == 3) && hit && hit->hitInterval == -1 && hit->hitCounts != -1 &&
        static_cast<int64_t>(state.m_hitTargets.size()) >= hit->hitCounts)
    {
        state.m_next = row.autoRelease == 3;
        world.destroy(entity);
    }
    follow(world, entity);
    if (state.m_hitInterval != "-1"_fx) state.m_hitInterval = std::max(Fixed{}, state.m_hitInterval - dt);
    if (ecs.get<EntityGroup>(entity).destroyRequested && state.m_next && !ecs.get<RoleHitState>(entity).m_obstruct)
    {
        state.m_next = false;
        if (row.nextEffectId != -1) spawnChild(world, entity, row.nextEffectId);
    }
}
/// 世界移除钩子终止运行叶子并清理动作资源。
void remove(World& world, Entity entity)
{
    if (!world.ecs().has<EffectTree>(entity)) return;
    auto tree = world.ecs().get<EffectTree>(entity);
    if (tree.m_instance.nodes.empty()) return;
    const auto def = definition(tree);
    Context ctx{world, entity, tree};
    BTRunner<Context, int32_t, std::variant<AttackEffect>>(def, registry(), tree.m_instance).exit(ctx);
}
/// 只将自动释放模式一且跟随的效果绑定角色生命周期。
void removeOwner(World& world, Entity owner)
{
    world.ecs().each<EffectState>([&](Entity entity, const EffectState& state) {
        const auto& row = conf::Config::instance().get<conf::EffectConfig>(state.m_configId);
        if (state.m_owner == owner && row.autoRelease == 1 && (row.follow == 1 || row.follow == 2)) world.destroy(entity);
    });
}
/// 将位移通知分派给特效当前动作。
bool onDisplacementEvent(World& world, Entity entity, DisplacementEvent event)
{
    const auto node = world.ecs().get<Displacement>(entity).listenerNode;
    auto& tree = world.ecs().get<EffectTree>(entity);
    if (node < 1 || static_cast<size_t>(node) > tree.m_actions.size()) return false;
    const auto* row = conf::Config::instance().find<conf::ActionEffectConfig>(tree.m_actions[node - 1]);
    return row && attack::displacementEvent(world, entity, std::get<AttackEffect>(tree.m_instance.leaves[node - 1]).m_action, common(*row), event);
}
/// 同步实际动画循环计数到当前动作。
void animationComplete(World& world, Entity entity, int64_t loops)
{
    auto& tree = world.ecs().get<EffectTree>(entity);
    for (size_t i = 0; i < tree.m_actions.size(); ++i)
        if (tree.m_instance.nodes[i + 1].status == BTStatus::Running)
        {
            const auto* row = conf::Config::instance().find<conf::ActionEffectConfig>(tree.m_actions[i]);
            if (row) attack::completeAnimation(std::get<AttackEffect>(tree.m_instance.leaves[i]).m_action, common(*row), loops);
        }
}
}
