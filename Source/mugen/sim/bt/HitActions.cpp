#include "HitActions.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/Hit.h"
#include "mugen/sim/hit/HitState.h"
#include "mugen/sim/status/Status.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::behavior::hit_actions
{
namespace
{
using A = ActionType;
using S = EntityRoleStatus;
// 查询角色配置，引用缺失时保留可观察的配置错误。
const conf::RoleConfig* role(BTCtx& ctx)
{
    return conf::Config::instance().find<conf::RoleConfig>(ctx.world.ecs().get<RoleIdentity>(ctx.entity).roleId);
}
// 按叶状态地址找到位移监听节点。
int32_t nodeFor(BTCtx& ctx, LeafState& state)
{
    auto& tree = ctx.world.ecs().get<RoleTree>(ctx.entity);
    const auto& topology = definition(tree.defKey).topology();
    for (uint32_t i = 0; i < topology.nodes().size(); ++i)
        if (topology.node(i).type == BTNodeType::Action && &tree.instance.leaves[topology.leafIndex(i)] == &state)
            return static_cast<int32_t>(i);
    return -1;
}
// 每次重新播放都重置动画完成计数。
void play(BTCtx& ctx, A type, HitActionState& leaf)
{
    auto& motion = ctx.world.ecs().get<MotionState>(ctx.entity);
    motion.motionName = animationName(type);
    motion.timeMs = {};
    motion.m_completedLoops = 0;
    motion.playing = true;
    motion.loop = type == A::Idle;
    motion.actionScale = type == A::HitUp ? conf::kHitUpScaleTime : type == A::HitDown ? conf::kHitDownScaleTime :
        type == A::HitFloor ? conf::kHitFloorScaleTime : type == A::GetUp ? conf::kGetUpScaleTime :
        type == A::Hit || type == A::HitSwitch || type == A::Controlled ? conf::kHitScaleTime : "1"_fx;
    leaf.m_animationEnd = false;
    if (type == A::Hit || type == A::Controlled)
        ctx.world.emit(ctx.entity, RoleIntensity{"1.3"_fx});
}
// 仅设置后继状态和完成标记，由正常组合节点退出清理旧状态。
void transition(BTCtx& ctx, HitActionState& leaf, S next)
{
    leaf.m_finished = true;
    status::mixStatus(ctx.world.ecs().get<RoleStatus>(ctx.entity), next);
}
// 死亡转换保留调用点指定的位移参数。
void death(BTCtx& ctx, HitActionState& leaf, int32_t displacement = -1)
{
    leaf.m_finished = true;
    status::dealWithStatus(ctx.world, ctx.entity, S::Death, displacement);
}
// 挣脱及普通硬直结束共用清位移和硬直逻辑。
void success(BTCtx& ctx, HitActionState& leaf, S next)
{
    if (hit::isDead(ctx.world, ctx.entity))
    {
        const auto* row = role(ctx);
        death(ctx, leaf, row ? row->deathDisplacementId : -1);
    }
    else
    {
        displacement::reset(ctx.world, ctx.entity);
        transition(ctx, leaf, next);
    }
    ctx.world.ecs().get<RoleHitState>(ctx.entity).stiffTime = {};
}
// 读取后清空待处理打击，配置零标识与无待处理记录严格区分。
const conf::SkillHitConfig* consumeHit(BTCtx& ctx)
{
    auto& id = ctx.world.ecs().get<RoleStatus>(ctx.entity).entityParams.hit_id;
    const auto value = id;
    id.reset();
    return value ? conf::Config::instance().find<conf::SkillHitConfig>(*value) : nullptr;
}
// 判断当前或前一次技能是否来自挣脱槽。
bool breakaway(BTCtx& ctx)
{
    const auto* pool = ctx.world.ecs().tryGet<SkillPool>(ctx.entity);
    if (!pool) return false;
    for (const auto index : {pool->m_current, pool->m_before})
        if (index != kNoSkill && skill::baseSlot(*pool, pool->m_skills.at(index)) == "L") return true;
    return false;
}
// 动画完成时按各独立动作的原始条件改变状态。
void completed(BTCtx& ctx, A type, HitActionState& leaf)
{
    if (leaf.m_animationEnd) return;
    const auto* row = role(ctx);
    switch (type)
    {
    case A::Hit: case A::Controlled:
    {
        const auto* data = conf::Config::instance().find<conf::SkillHitConfig>(leaf.m_hitId);
        if (!data) return;
        if (data->hitType < 0 || data->hitType > 2) return;
        leaf.m_animationEnd = true;
        if (data->hitType != 0 && row && row->hitDisplacementId == -1 && !leaf.m_break)
            transition(ctx, leaf, S::HitUp);
        ctx.world.emit(ctx.entity, RoleIntensity{"1"_fx});
        break;
    }
    case A::HitSwitch:
        leaf.m_animationEnd = leaf.m_finished = true;
        status::abortStatus(ctx.world.ecs().get<RoleStatus>(ctx.entity), S::HitSwitch);
        if (hit::isDead(ctx.world, ctx.entity)) death(ctx, leaf);
        break;
    case A::HitUp:
        leaf.m_animationEnd = true;
        if (leaf.m_displacementEnd && !leaf.m_break) transition(ctx, leaf, S::HitDown);
        break;
    case A::HitDown:
        if (leaf.m_break) break;
        leaf.m_animationEnd = true;
        if (leaf.m_displacementEnd) transition(ctx, leaf, S::HitFloor);
        break;
    case A::HitFloor:
        leaf.m_animationEnd = true;
        leaf.m_time = {};
        if (leaf.m_displacementEnd && hit::isDead(ctx.world, ctx.entity)) death(ctx, leaf);
        break;
    case A::GetUp:
        leaf.m_animationEnd = true;
        if (hit::isDead(ctx.world, ctx.entity)) death(ctx, leaf, row ? row->deathDisplacementId : -1);
        else transition(ctx, leaf, S::Wake);
        break;
    case A::Death:
        leaf.m_animationEnd = true;
        leaf.m_time = {};
        break;
    default: break;
    }
}
// 地面受击重新命中保留动画未完成时的延迟挑起逻辑。
void groundChase(BTCtx& ctx, LeafState& state, const conf::SkillHitConfig& data)
{
    auto& leaf = std::get<HitActionState>(state);
    const auto* row = role(ctx);
    if (!row) return;
    leaf.m_hitId = data.id;
    leaf.m_displacementEnd = false;
    leaf.m_displacementId = row->hitDisplacementId;
    if (leaf.m_displacementId == -1)
    {
        leaf.m_displacementId = data.displacementId;
        if (data.hitType == 0)
        {
            if (ctx.world.ecs().get<Transform>(ctx.entity).position.y <= Fixed{}) play(ctx, A::Hit, leaf);
            else leaf.m_displacementId = data.airDisplacementId;
        }
        else if (leaf.m_animationEnd && !leaf.m_break) transition(ctx, leaf, S::HitUp);
    }
    else
    {
        play(ctx, A::Hit, leaf);
        if (leaf.m_displacementId == 0) leaf.m_displacementId = -1;
        else if (data.hitType == 0) leaf.m_displacementId = data.displacementId;
    }
    displacement::set(ctx.world, ctx.entity, leaf.m_displacementId, nodeFor(ctx, state));
}
// 浮空和倒地追击先重设位移再扣浮空值，最后决定能否再次挑起。
void airChase(BTCtx& ctx, A type, LeafState& state, const conf::SkillHitConfig& data)
{
    auto& leaf = std::get<HitActionState>(state);
    leaf.m_displacementId = data.hitType == 0 ? data.airDisplacementId : data.displacementId;
    displacement::set(ctx.world, ctx.entity, leaf.m_displacementId, nodeFor(ctx, state));
    if (data.hitType != 1) hit::dealWithWeight(ctx.world, ctx.entity, data.hitRigidity);
    if (type == A::HitUp)
    {
        if (data.hitType == 1 && !leaf.m_break) transition(ctx, leaf, S::HitDown);
        if (data.hitType == 0 || data.hitType == 2) leaf.m_displacementEnd = false;
        return;
    }
    const bool knockDown = type == A::HitFloor && data.hitType == 1 && !leaf.m_break;
    const bool relaunch = (data.hitType == 0 || data.hitType == 2) &&
        !hit::isRigidity(ctx.world, ctx.entity) && !leaf.m_break;
    if (knockDown) transition(ctx, leaf, S::HitDown);
    if (relaunch) transition(ctx, leaf, S::HitUp);
    if (type == A::HitFloor && (knockDown || relaunch))
        ctx.world.ecs().get<RoleHitState>(ctx.entity).stiffTime = Fixed::fromInt(data.stiffTime) *
            ("1"_fx + ctx.world.ecs().get<Attributes>(ctx.entity).extend[15]);
}
}
// 动作进入分别初始化动画、位移、保护请求和生命周期副作用。
void enter(BTCtx& ctx, const Param& param, LeafState& state)
{
    state = HitActionState{};
    auto& leaf = std::get<HitActionState>(state);
    const auto type = std::get<A>(param);
    const auto* row = role(ctx);
    behaviorStateStart(ctx.world, ctx.entity, type);
    if (type == A::Destroy) { ctx.world.destroy(ctx.entity); return; }
    if (type == A::Revive)
    {
        if (ctx.world.hitHooks().m_roleAbilities) ctx.world.hitHooks().m_roleAbilities(ctx.world, ctx.entity, true);
        auto& vitals = ctx.world.ecs().get<Vitals>(ctx.entity);
        vitals.hp = vitals.maxHp;
        ctx.world.ecs().get<RoleStatus>(ctx.entity).eEntityStatus = S::Idle;
        return;
    }
    const auto& extra = ctx.world.ecs().get<ExtraState>(ctx.entity);
    if (type != A::Controlled || extra.active(mugen::ExtraState::Stun) || !extra.active(mugen::ExtraState::Palsy))
        play(ctx, type == A::Wake ? A::Idle : type, leaf);
    const bool hero = ctx.world.ecs().get<RoleIdentity>(ctx.entity).roleType == 1;
    if (type == A::Wake)
    {
        const int32_t id = hero ? conf::kRoleHeroWakeBuffId : conf::kRoleEnemyWakeBuffId;
        if (id != -1) buff::addEntityBuff(ctx.world, ctx.entity, id);
        if (row) ctx.world.ecs().get<RoleHitState>(ctx.entity).rigidity = Fixed::fromInt(row->rigidity);
        if (hero) hit::resetHitProtect(ctx.world, ctx.entity);
        return;
    }
    if (type == A::GetUp)
    {
        if (hero) { buff::addEntityBuff(ctx.world, ctx.entity, 156); buff::addEntityBuff(ctx.world, ctx.entity, 154); }
        return;
    }
    if (type == A::HitSwitch) return;
    if (type == A::Hit || type == A::Controlled)
    {
        const auto* data = consumeHit(ctx);
        if (row && data)
        {
            leaf.m_hitId = data->id;
            leaf.m_displacementId = row->hitDisplacementId == 0 ? -1 :
                row->hitDisplacementId == -1 || data->hitType == 0 ? data->displacementId : row->hitDisplacementId;
        }
    }
    if (type == A::Death)
    {
        leaf.m_displacementId = ctx.world.ecs().get<RoleStatus>(ctx.entity).entityParams.death_displacement_id;
        if (ctx.world.hitHooks().m_roleAbilities) ctx.world.hitHooks().m_roleAbilities(ctx.world, ctx.entity, false);
        const auto* pool = ctx.world.ecs().tryGet<SkillPool>(ctx.entity);
        if (pool && pool->m_crazy)
        {
            ctx.world.ecs().get<Vitals>(ctx.entity).ep = {};
            skill::updateCrazy(ctx.world, ctx.entity);
        }
    }
    displacement::set(ctx.world, ctx.entity, leaf.m_displacementId, nodeFor(ctx, state));
}
// 每帧按原始顺序处理挣脱、硬直完成和追击。
BTStatus tick(BTCtx& ctx, const Param& param, LeafState& state)
{
    auto& leaf = std::get<HitActionState>(state);
    const auto type = std::get<A>(param);
    leaf.m_time += ctx.world.ecs().singleton<WorldClock>().dtMs;
    if (type == A::Destroy) return BTStatus::Success;
    if (type == A::Revive) return BTStatus::Running;
    if (type == A::Wake)
    {
        if (hit::isDead(ctx.world, ctx.entity)) { const auto* row = role(ctx); death(ctx, leaf, row ? row->deathDisplacementId : -1); }
        return BTStatus::Success;
    }
    if (type == A::Death)
    {
        if (leaf.m_animationEnd && (leaf.m_displacementEnd || leaf.m_displacementId == -1) &&
            leaf.m_time > Fixed::fromInt(conf::kRoleDeathTime))
        {
            ctx.world.ecs().singleton<RoomState>().m_roleDeaths.push_back(ctx.entity);
            ctx.world.emit(ctx.entity, RoleDeath{ctx.world.ecs().get<RoleIdentity>(ctx.entity).roleId});
            if (ctx.world.ecs().get<RoleIdentity>(ctx.entity).roleType != 1)
            {
                leaf.m_finished = true;
                const auto* row = role(ctx);
                if (row && row->deathEffectId != -1)
                {
                    const auto source = ctx.world.ecs().get<Transform>(ctx.entity);
                    const auto effectEntity = effect::create(ctx.world, ctx.entity, row->deathEffectId);
                    if (effectEntity.value != 0)
                    {
                        auto& target = ctx.world.ecs().get<Transform>(effectEntity);
                        target.position = source.position;
                        target.towardX = source.towardX;
                        target.vectorX = Fixed::fromInt(source.towardX);
                        target.vectorZ = "1"_fx;
                        const auto& effectRow = conf::Config::instance().get<conf::EffectConfig>(row->deathEffectId);
                        if (effectRow.relativePosition.size() >= 3)
                        {
                            target.position.x += Fixed::fromInt(source.towardX) * effectRow.relativePosition[0];
                            target.position.y += effectRow.relativePosition[1];
                            target.position.z += effectRow.relativePosition[2];
                        }
                        ctx.world.ecs().get<EffectState>(effectEntity).m_hitEntity = ctx.entity;
                        effect::enter(ctx.world, effectEntity);
                    }
                }
                if (ctx.world.hitHooks().m_dropGoods) ctx.world.hitHooks().m_dropGoods(ctx.world, ctx.entity);
            }
        }
        return leaf.m_finished ? BTStatus::Success : BTStatus::Running;
    }
    if (type != A::GetUp && breakaway(ctx)) { leaf.m_break = true; success(ctx, leaf, S::Attack); }
    if (type == A::Hit && ctx.world.ecs().get<RoleIdentity>(ctx.entity).roleType == 1 &&
        ctx.world.ecs().get<RoleHitState>(ctx.entity).m_hitProtectExecuted) transition(ctx, leaf, S::HitDown);
    bool timed = false;
    if ((type == A::Hit || type == A::Controlled) && !leaf.m_break)
    {
        const auto* data = conf::Config::instance().find<conf::SkillHitConfig>(leaf.m_hitId);
        const auto* row = role(ctx);
        if (data && row && (data->hitType == 0 || leaf.m_displacementId == -1 || row->hitDisplacementId != -1) &&
            leaf.m_animationEnd && (leaf.m_displacementEnd || leaf.m_displacementId == -1) &&
            leaf.m_time >= hit::getStiffTime(ctx.world, ctx.entity))
        { success(ctx, leaf, S::HitSwitch); timed = true; }
    }
    if (type == A::HitFloor && leaf.m_animationEnd && leaf.m_displacementEnd && !leaf.m_break &&
        leaf.m_time >= hit::getStiffTime(ctx.world, ctx.entity))
    {
        if (hit::isDead(ctx.world, ctx.entity)) death(ctx, leaf);
        else transition(ctx, leaf, S::GetUp);
        ctx.world.ecs().get<RoleHitState>(ctx.entity).stiffTime = {};
        timed = true;
    }
    if (!timed && ctx.world.ecs().get<RoleStatus>(ctx.entity).entityParams.hit_id.has_value())
    {
        if (type == A::HitSwitch) transition(ctx, leaf, S::Hit);
        else if (type == A::Hit || type == A::Controlled || type == A::HitUp || type == A::HitDown || type == A::HitFloor)
        {
            const auto* data = consumeHit(ctx);
            if (data)
            {
                if (type == A::Hit || type == A::Controlled) groundChase(ctx, state, *data);
                else airChase(ctx, type, state, *data);
            }
        }
    }
    return leaf.m_finished ? BTStatus::Success : BTStatus::Running;
}
// 地面击退和死亡退出才清理位移，其余状态继承运动。
void exit(BTCtx& ctx, const Param& param, LeafState& state)
{
    const auto type = std::get<A>(param);
    const auto& leaf = std::get<HitActionState>(state);
    const auto* data = conf::Config::instance().find<conf::SkillHitConfig>(leaf.m_hitId);
    if (type == A::Death || ((type == A::Hit || type == A::Controlled) && data && data->hitType == 0 &&
        ctx.world.ecs().get<Transform>(ctx.entity).position.y == Fixed{})) displacement::reset(ctx.world, ctx.entity);
    behaviorStateEnd(ctx.world, ctx.entity, type);
}
// 位移事件只驱动本叶子的完成条件，不抢占行为树。
bool onDisplacementEvent(BTCtx& ctx, const Param& param, LeafState& state)
{
    auto& leaf = std::get<HitActionState>(state);
    const auto type = std::get<A>(param);
    const auto event = ctx.event;
    if (type == A::Hit || type == A::Controlled)
    {
        if (event == DisplacementEvent::Brake && !leaf.m_displacementEnd)
        { leaf.m_displacementEnd = true; leaf.m_time = {}; }
        else if (event == DisplacementEvent::Air && !leaf.m_displacementEnd && !leaf.m_break)
        { leaf.m_displacementEnd = true; if (!leaf.m_animationEnd) transition(ctx, leaf, S::HitDown); }
    }
    else if (type == A::HitUp || type == A::HitDown)
    {
        if ((event == DisplacementEvent::Lie || (type == A::HitUp && event == DisplacementEvent::Air)) &&
            !leaf.m_displacementEnd && (type == A::HitUp || !leaf.m_break))
        {
            leaf.m_displacementEnd = true;
            if (leaf.m_animationEnd && !leaf.m_break) transition(ctx, leaf, type == A::HitUp ? S::HitDown : S::HitFloor);
        }
        if (event == DisplacementEvent::Left || event == DisplacementEvent::Right ||
            event == DisplacementEvent::Top || event == DisplacementEvent::Bottom) return false;
        return hit::isRigidity(ctx.world, ctx.entity);
    }
    else if (type == A::HitFloor || type == A::Death)
    {
        if ((event == DisplacementEvent::Brake || event == DisplacementEvent::Lie) && !leaf.m_displacementEnd)
        {
            leaf.m_displacementEnd = true;
            leaf.m_time = {};
            if (type == A::HitFloor && leaf.m_animationEnd && hit::isDead(ctx.world, ctx.entity)) death(ctx, leaf);
        }
        else if (type == A::HitFloor && event == DisplacementEvent::Floor) return hit::isRigidity(ctx.world, ctx.entity);
    }
    return false;
}
// 只向当前运行的受击叶子发送真实动画完成通知。
void animationComplete(World& world, Entity entity)
{
    auto& tree = world.ecs().get<RoleTree>(entity);
    const auto& def = definition(tree.defKey);
    const auto& topology = def.topology();
    BTCtx ctx{world, entity};
    for (uint32_t i = 0; i < topology.nodes().size(); ++i)
    {
        const auto& node = topology.node(i);
        if (node.type != BTNodeType::Action || node.leafTypeId != kHitStateLeaf ||
            tree.instance.nodes[i].status != BTStatus::Running) continue;
        auto& leaf = std::get<HitActionState>(tree.instance.leaves[topology.leafIndex(i)]);
        completed(ctx, std::get<A>(def.param(node.paramIndex)), leaf);
    }
}
}
