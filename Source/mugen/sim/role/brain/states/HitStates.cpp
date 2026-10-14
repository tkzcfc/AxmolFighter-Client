#include "HitStates.h"

#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/brain/RoleBranches.h"
#include "mugen/sim/skill/Skill.h"

namespace mugen::sim::role::brain::states
{
namespace
{
using A = behavior::ActionType;
using S = EntityRoleStatus;
// 查询角色配置，引用缺失时保留可观察的配置错误。
const conf::RoleConfig* role(RoleContext& ctx)
{
    return conf::Config::instance().find<conf::RoleConfig>(ctx.get<RoleIdentity>().roleId);
}
// 每次重新播放都重置动画完成计数。
void play(RoleContext& ctx, A type, behavior::HitActionState& leaf)
{
    auto& motion         = ctx.get<MotionState>();
    motion.motionName    = behavior::animationName(type);
    motion.timeMs        = {};
    motion.m_completedLoops = 0;
    motion.playing       = true;
    motion.loop          = type == A::Idle;
    motion.actionScale   = type == A::HitUp ? conf::kHitUpScaleTime : type == A::HitDown ? conf::kHitDownScaleTime :
        type == A::HitFloor ? conf::kHitFloorScaleTime : type == A::GetUp ? conf::kGetUpScaleTime :
        type == A::Hit || type == A::HitSwitch || type == A::Controlled ? conf::kHitScaleTime : "1"_fx;
    leaf.m_animationEnd = false;
    if (type == A::Hit || type == A::Controlled)
        ctx.world().emit(ctx.entity(), RoleIntensity{"1.3"_fx});
}
// 仅设置后继状态和完成标记，由正常组合节点退出清理旧状态。
void transition(RoleContext& ctx, behavior::HitActionState& leaf, S next)
{
    leaf.m_finished = true;
    ctx.get<RoleStatus>().mix(next);
}
// 死亡转换保留调用点指定的位移参数。
void death(RoleContext& ctx, behavior::HitActionState& leaf, int32_t displacement = -1)
{
    leaf.m_finished = true;
    ctx.role().changeStatus(S::Death, displacement);
}
// 挣脱及普通硬直结束共用清位移和硬直逻辑。
void success(RoleContext& ctx, behavior::HitActionState& leaf, S next)
{
    if (ctx.role().isDead())
    {
        const auto* row = role(ctx);
        death(ctx, leaf, row ? row->deathDisplacementId : -1);
    }
    else
    {
        displacement::reset(ctx.world(), ctx.entity());
        transition(ctx, leaf, next);
    }
    ctx.get<RoleHitState>().stiffTime = {};
}
// 读取后清空待处理打击，配置零标识与无待处理记录严格区分。
const conf::SkillHitConfig* consumeHit(RoleContext& ctx)
{
    auto& id = ctx.get<RoleStatus>().entityParams.hit_id;
    const auto value = id;
    id.reset();
    return value ? conf::Config::instance().find<conf::SkillHitConfig>(*value) : nullptr;
}
// 判断当前或前一次技能是否来自挣脱槽。
bool breakaway(RoleContext& ctx)
{
    const auto* pool = ctx.world().ecs().tryGet<SkillPool>(ctx.entity());
    if (!pool)
        return false;
    for (const auto index : {pool->m_current, pool->m_before})
        if (index != kNoSkill && skill::baseSlot(*pool, pool->m_skills.at(index)) == "L")
            return true;
    return false;
}
// 动画完成时按各独立动作的原始条件改变状态。
void completed(RoleContext& ctx, A type, behavior::HitActionState& leaf)
{
    if (leaf.m_animationEnd)
        return;
    const auto* row = role(ctx);
    switch (type)
    {
    case A::Hit:
    case A::Controlled:
    {
        const auto* data = conf::Config::instance().find<conf::SkillHitConfig>(leaf.m_hitId);
        if (!data)
            return;
        if (data->hitType < 0 || data->hitType > 2)
            return;
        leaf.m_animationEnd = true;
        if (data->hitType != 0 && row && row->hitDisplacementId == -1 && !leaf.m_break)
            transition(ctx, leaf, S::HitUp);
        ctx.world().emit(ctx.entity(), RoleIntensity{"1"_fx});
        break;
    }
    case A::HitSwitch:
        leaf.m_animationEnd = leaf.m_finished = true;
        ctx.get<RoleStatus>().abort(S::HitSwitch);
        if (ctx.role().isDead())
            death(ctx, leaf);
        break;
    case A::HitUp:
        leaf.m_animationEnd = true;
        if (leaf.m_displacementEnd && !leaf.m_break)
            transition(ctx, leaf, S::HitDown);
        break;
    case A::HitDown:
        if (leaf.m_break)
            break;
        leaf.m_animationEnd = true;
        if (leaf.m_displacementEnd)
            transition(ctx, leaf, S::HitFloor);
        break;
    case A::HitFloor:
        leaf.m_animationEnd = true;
        leaf.m_time         = {};
        if (leaf.m_displacementEnd && ctx.role().isDead())
            death(ctx, leaf);
        break;
    case A::GetUp:
        leaf.m_animationEnd = true;
        if (ctx.role().isDead())
            death(ctx, leaf, row ? row->deathDisplacementId : -1);
        else
            transition(ctx, leaf, S::Wake);
        break;
    case A::Death:
        leaf.m_animationEnd = true;
        leaf.m_time         = {};
        break;
    default:
        break;
    }
}
// 地面受击重新命中保留动画未完成时的延迟挑起逻辑。
void groundChase(RoleContext& ctx, behavior::HitActionState& leaf, const conf::SkillHitConfig& data)
{
    const auto* row = role(ctx);
    if (!row)
        return;
    leaf.m_hitId          = data.id;
    leaf.m_displacementEnd = false;
    leaf.m_displacementId = row->hitDisplacementId;
    if (leaf.m_displacementId == -1)
    {
        leaf.m_displacementId = data.displacementId;
        if (data.hitType == 0)
        {
            if (ctx.get<Transform>().position.y <= Fixed{})
                play(ctx, A::Hit, leaf);
            else
                leaf.m_displacementId = data.airDisplacementId;
        }
        else if (leaf.m_animationEnd && !leaf.m_break)
            transition(ctx, leaf, S::HitUp);
    }
    else
    {
        play(ctx, A::Hit, leaf);
        if (leaf.m_displacementId == 0)
            leaf.m_displacementId = -1;
        else if (data.hitType == 0)
            leaf.m_displacementId = data.displacementId;
    }
    displacement::set(ctx.world(), ctx.entity(), leaf.m_displacementId, ctx.node());
}
// 浮空和倒地追击先重设位移再扣浮空值，最后决定能否再次挑起。
void airChase(RoleContext& ctx, A type, behavior::HitActionState& leaf, const conf::SkillHitConfig& data)
{
    leaf.m_displacementId = data.hitType == 0 ? data.airDisplacementId : data.displacementId;
    displacement::set(ctx.world(), ctx.entity(), leaf.m_displacementId, ctx.node());
    if (data.hitType != 1)
        ctx.role().applyWeight(data.hitRigidity);
    if (type == A::HitUp)
    {
        if (data.hitType == 1 && !leaf.m_break)
            transition(ctx, leaf, S::HitDown);
        if (data.hitType == 0 || data.hitType == 2)
            leaf.m_displacementEnd = false;
        return;
    }
    const bool knockDown = type == A::HitFloor && data.hitType == 1 && !leaf.m_break;
    const bool relaunch  = (data.hitType == 0 || data.hitType == 2) &&
        !ctx.role().isRigidity() && !leaf.m_break;
    if (knockDown)
        transition(ctx, leaf, S::HitDown);
    if (relaunch)
        transition(ctx, leaf, S::HitUp);
    if (type == A::HitFloor && (knockDown || relaunch))
        ctx.get<RoleHitState>().stiffTime = Fixed::fromInt(data.stiffTime) *
            ("1"_fx + ctx.get<Attributes>().extend[15]);
}
}  // namespace

void HitState::enter(RoleContext& ctx, Param type, State& leaf)
{
    leaf = behavior::HitActionState{};
    const auto* row = role(ctx);
    behavior::behaviorStateStart(ctx.world(), ctx.entity(), type);
    if (type == A::Destroy)
    {
        ctx.world().destroy(ctx.entity());
        return;
    }
    if (type == A::Revive)
    {
        if (ctx.world().hitHooks().m_roleAbilities)
            ctx.world().hitHooks().m_roleAbilities(ctx.world(), ctx.entity(), true);
        auto& vitals = ctx.get<Vitals>();
        vitals.hp    = vitals.maxHp;
        ctx.get<RoleStatus>().eEntityStatus = S::Idle;
        return;
    }
    const auto& extra = ctx.get<ExtraState>();
    if (type != A::Controlled || extra.active(mugen::ExtraState::Stun) || !extra.active(mugen::ExtraState::Palsy))
        play(ctx, type == A::Wake ? A::Idle : type, leaf);
    const bool hero = ctx.isHero();
    if (type == A::Wake)
    {
        const int32_t id = hero ? conf::kRoleHeroWakeBuffId : conf::kRoleEnemyWakeBuffId;
        if (id != -1)
            buff::addEntityBuff(ctx.world(), ctx.entity(), id);
        if (row)
            ctx.get<RoleHitState>().rigidity = Fixed::fromInt(row->rigidity);
        if (hero)
            ctx.role().resetHitProtect();
        return;
    }
    if (type == A::GetUp)
    {
        if (hero)
        {
            buff::addEntityBuff(ctx.world(), ctx.entity(), 156);
            buff::addEntityBuff(ctx.world(), ctx.entity(), 154);
        }
        return;
    }
    if (type == A::HitSwitch)
        return;
    if (type == A::Hit || type == A::Controlled)
    {
        const auto* data = consumeHit(ctx);
        if (row && data)
        {
            leaf.m_hitId          = data->id;
            leaf.m_displacementId = row->hitDisplacementId == 0 ? -1 :
                row->hitDisplacementId == -1 || data->hitType == 0 ? data->displacementId : row->hitDisplacementId;
        }
    }
    if (type == A::Death)
    {
        leaf.m_displacementId = ctx.get<RoleStatus>().entityParams.death_displacement_id;
        if (ctx.world().hitHooks().m_roleAbilities)
            ctx.world().hitHooks().m_roleAbilities(ctx.world(), ctx.entity(), false);
        const auto* pool = ctx.world().ecs().tryGet<SkillPool>(ctx.entity());
        if (pool && pool->m_crazy)
        {
            ctx.get<Vitals>().ep = {};
            skill::updateCrazy(ctx.world(), ctx.entity());
        }
    }
    displacement::set(ctx.world(), ctx.entity(), leaf.m_displacementId, ctx.node());
}

BTStatus HitState::update(RoleContext& ctx, Param type, State& leaf)
{
    leaf.m_time += ctx.dtMs();
    if (type == A::Destroy)
        return BTStatus::Success;
    if (type == A::Revive)
        return BTStatus::Running;
    if (type == A::Wake)
    {
        if (ctx.role().isDead())
        {
            const auto* row = role(ctx);
            death(ctx, leaf, row ? row->deathDisplacementId : -1);
        }
        return BTStatus::Success;
    }
    if (type == A::Death)
    {
        if (leaf.m_animationEnd && (leaf.m_displacementEnd || leaf.m_displacementId == -1) &&
            leaf.m_time > Fixed::fromInt(conf::kRoleDeathTime))
        {
            ctx.world().ecs().singleton<RoomState>().m_roleDeaths.push_back(ctx.entity());
            ctx.world().emit(ctx.entity(), RoleDeath{ctx.get<RoleIdentity>().roleId});
            if (ctx.get<RoleIdentity>().roleType != 1)
            {
                leaf.m_finished = true;
                const auto* row = role(ctx);
                if (row && row->deathEffectId != -1)
                {
                    const auto source = ctx.get<Transform>();
                    const auto effectEntity = effect::create(ctx.world(), ctx.entity(), row->deathEffectId);
                    if (effectEntity.value != 0)
                    {
                        auto& target             = ctx.world().ecs().get<Transform>(effectEntity);
                        target.position          = source.position;
                        target.towardX           = source.towardX;
                        target.vectorX           = Fixed::fromInt(source.towardX);
                        target.vectorZ           = "1"_fx;
                        const auto& effectRow = conf::Config::instance().get<conf::EffectConfig>(row->deathEffectId);
                        if (effectRow.relativePosition.size() >= 3)
                        {
                            target.position.x += Fixed::fromInt(source.towardX) * effectRow.relativePosition[0];
                            target.position.y += effectRow.relativePosition[1];
                            target.position.z += effectRow.relativePosition[2];
                        }
                        ctx.world().ecs().get<EffectState>(effectEntity).m_hitEntity = ctx.entity();
                        effect::enter(ctx.world(), effectEntity);
                    }
                }
                if (ctx.world().hitHooks().m_dropGoods)
                    ctx.world().hitHooks().m_dropGoods(ctx.world(), ctx.entity());
            }
        }
        return leaf.m_finished ? BTStatus::Success : BTStatus::Running;
    }
    if (type != A::GetUp && breakaway(ctx))
    {
        leaf.m_break = true;
        success(ctx, leaf, S::Attack);
    }
    if (type == A::Hit && ctx.isHero() && ctx.get<RoleHitState>().m_hitProtectExecuted)
        transition(ctx, leaf, S::HitDown);
    bool timed = false;
    if ((type == A::Hit || type == A::Controlled) && !leaf.m_break)
    {
        const auto* data = conf::Config::instance().find<conf::SkillHitConfig>(leaf.m_hitId);
        const auto* row  = role(ctx);
        if (data && row && (data->hitType == 0 || leaf.m_displacementId == -1 || row->hitDisplacementId != -1) &&
            leaf.m_animationEnd && (leaf.m_displacementEnd || leaf.m_displacementId == -1) &&
            leaf.m_time >= ctx.role().getStiffTime())
        {
            success(ctx, leaf, S::HitSwitch);
            timed = true;
        }
    }
    if (type == A::HitFloor && leaf.m_animationEnd && leaf.m_displacementEnd && !leaf.m_break &&
        leaf.m_time >= ctx.role().getStiffTime())
    {
        if (ctx.role().isDead())
            death(ctx, leaf);
        else
            transition(ctx, leaf, S::GetUp);
        ctx.get<RoleHitState>().stiffTime = {};
        timed = true;
    }
    if (!timed && ctx.get<RoleStatus>().entityParams.hit_id.has_value())
    {
        if (type == A::HitSwitch)
            transition(ctx, leaf, S::Hit);
        else if (type == A::Hit || type == A::Controlled || type == A::HitUp || type == A::HitDown || type == A::HitFloor)
        {
            const auto* data = consumeHit(ctx);
            if (data)
            {
                if (type == A::Hit || type == A::Controlled)
                    groundChase(ctx, leaf, *data);
                else
                    airChase(ctx, type, leaf, *data);
            }
        }
    }
    return leaf.m_finished ? BTStatus::Success : BTStatus::Running;
}

void HitState::exit(RoleContext& ctx, Param type, State& leaf)
{
    const auto* data = conf::Config::instance().find<conf::SkillHitConfig>(leaf.m_hitId);
    if (type == A::Death || ((type == A::Hit || type == A::Controlled) && data && data->hitType == 0 &&
        ctx.get<Transform>().position.y == Fixed{}))
        displacement::reset(ctx.world(), ctx.entity());
    behavior::behaviorStateEnd(ctx.world(), ctx.entity(), type);
}

bool HitState::onDisplacement(RoleContext& ctx, Param type, State& leaf)
{
    const auto event = ctx.event();
    if (type == A::Hit || type == A::Controlled)
    {
        if (event == DisplacementEvent::Brake && !leaf.m_displacementEnd)
        {
            leaf.m_displacementEnd = true;
            leaf.m_time            = {};
        }
        else if (event == DisplacementEvent::Air && !leaf.m_displacementEnd && !leaf.m_break)
        {
            leaf.m_displacementEnd = true;
            if (!leaf.m_animationEnd)
                transition(ctx, leaf, S::HitDown);
        }
    }
    else if (type == A::HitUp || type == A::HitDown)
    {
        if ((event == DisplacementEvent::Lie || (type == A::HitUp && event == DisplacementEvent::Air)) &&
            !leaf.m_displacementEnd && (type == A::HitUp || !leaf.m_break))
        {
            leaf.m_displacementEnd = true;
            if (leaf.m_animationEnd && !leaf.m_break)
                transition(ctx, leaf, type == A::HitUp ? S::HitDown : S::HitFloor);
        }
        if (event == DisplacementEvent::Left || event == DisplacementEvent::Right ||
            event == DisplacementEvent::Top || event == DisplacementEvent::Bottom)
            return false;
        return ctx.role().isRigidity();
    }
    else if (type == A::HitFloor || type == A::Death)
    {
        if ((event == DisplacementEvent::Brake || event == DisplacementEvent::Lie) && !leaf.m_displacementEnd)
        {
            leaf.m_displacementEnd = true;
            leaf.m_time            = {};
            if (type == A::HitFloor && leaf.m_animationEnd && ctx.role().isDead())
                death(ctx, leaf);
        }
        else if (type == A::HitFloor && event == DisplacementEvent::Floor)
            return ctx.role().isRigidity();
    }
    return false;
}

// 只向当前运行的受击分支发送真实动画完成通知。
void animationComplete(World& world, Entity entity)
{
    auto& brain = world.ecs().get<RoleBrain>(entity);
    if (brain.m_phase != BrainPhase::Running)
        return;
    switch (brain.m_branch)
    {
    case RoleBranch::Death:
    case RoleBranch::Revive:
    case RoleBranch::Wake:
    case RoleBranch::GetUp:
    case RoleBranch::HitFloor:
    case RoleBranch::HitDown:
    case RoleBranch::HitUp:
    case RoleBranch::HitSwitch:
    case RoleBranch::Hit:
    case RoleBranch::Controlled:
    {
        RoleContext ctx{world, entity};
        const A type = brain.m_branch == RoleBranch::Death && brain.m_destroyStage == 1
                           ? A::Destroy
                           : branchAction(brain.m_branch);
        completed(ctx, type, std::get<behavior::HitActionState>(brain.m_data));
        break;
    }
    default:
        break;
    }
}
}  // namespace mugen::sim::role::brain::states
