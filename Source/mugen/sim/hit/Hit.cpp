#include "Hit.h"
#include "HitSeams.h"
#include "HitState.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::hit
{
namespace
{
// 判断实体是否仍可参与逻辑命中。
bool enabled(World& world, Entity entity)
{
    const auto* group = world.ecs().tryGet<EntityGroup>(entity);
    return group && !group->destroyRequested;
}
// 判断当前技能是否为闪避槽。
bool dodging(World& world, Entity entity)
{
    const auto* pool = world.ecs().tryGet<SkillPool>(entity);
    return pool && pool->m_current != kNoSkill && skill::baseSlot(*pool, pool->m_skills.at(pool->m_current)) == "F";
}
// 构造各命中阶段共享的完整来源数据。
HitTrigger context(World& world, Entity effect, Entity target)
{
    const auto source = world.ecs().get<EffectState>(effect);
    HitTrigger result;
    result.m_attacker       = source.m_owner;
    result.m_target         = target;
    result.m_effect         = effect;
    result.m_skillId        = source.m_skillId;
    result.m_baseSkillId    = source.m_baseSkillId;
    result.m_slot           = source.m_slot;
    result.m_skillAttribute = source.m_skillAttributeTag;
    result.m_artifactId     = source.m_artifactId;
    result.m_artifactIndex  = source.m_artifactIndex;
    return result;
}
// 属性闪避与主动闪避分别触发事件和文本。
void dodge(World& world, Entity target, const HitTrigger& data, bool passive)
{
    buff::trigger(world, target, passive ? BFEvent::AttackMiss : BFEvent::EvadeSuccess, data);
    world.emit(target, Tips{passive ? "MISS" : "DODGE"});
}
// 生成或复用同配置同目标的命中特效，位置取碰撞接触点。
void hitEffects(World& world, Entity sourceEntity, Entity target, FixedVec2 contact)
{
    auto& ecs         = world.ecs();
    const auto source = ecs.get<EffectState>(sourceEntity);
    const auto* row   = conf::Config::instance().find<conf::EffectConfig>(source.m_configId);
    if (!row)
        return;
    const auto transform = ecs.get<Transform>(sourceEntity);
    const auto z         = ecs.get<Transform>(target).position.z;
    for (const auto id : row->hitEffectIds)
    {
        if (id == -1)
            continue;
        const auto* childRow = conf::Config::instance().find<conf::EffectConfig>(id);
        if (!childRow)
            continue;
        Entity child;
        ecs.each<EffectState>([&](Entity entity, const EffectState& state) {
            if (child.value == 0 && state.m_configId == id && state.m_hitEntity == target && enabled(world, entity))
                child = entity;
        });
        if (child.value == 0)
        {
            child = effect::create(world, source.m_owner, id);
            if (child.value == 0)
                continue;
            if (childRow->nextEffectId == -1)
                ecs.get<EffectState>(child).m_hitEntity = target;
        }
        auto& state               = ecs.get<EffectState>(child);
        state.m_skillId           = source.m_skillId;
        state.m_baseSkillId       = source.m_baseSkillId;
        state.m_slot              = source.m_slot;
        state.m_skillAttributeTag = source.m_skillAttributeTag;
        state.m_artifactId        = source.m_artifactId;
        state.m_artifactIndex     = source.m_artifactIndex;
        state.m_capabilityTrigger = source.m_capabilityTrigger;
        auto& position            = ecs.get<Transform>(child);
        position.vectorX          = transform.vectorX;
        position.vectorZ          = transform.vectorZ;
        position.towardX          = transform.towardX;
        position.position         = {contact.x, contact.y - z, z - "1"_fx};
        effect::enter(world, child);
    }
}
// 受击预处理保持事件触发与状态过滤的先后顺序。
bool beforeHit(World& world,
               Entity target,
               const HitTrigger& data,
               const conf::EffectConfig& effectRow,
               const conf::SkillHitConfig& hitRow,
               FixedVec2 contact)
{
    auto& ecs       = world.ecs();
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ecs.get<RoleIdentity>(target).roleId);
    if (row && row->hurtNumPos.size() >= 2)
    {
        contact.x -= ecs.get<Transform>(data.m_effect).vectorX * Fixed::fromInt(row->hurtNumPos[0]);
        contact.y += Fixed::fromInt(row->hurtNumPos[1]);
    }
    ecs.get<RoleHitState>(target).m_contact = contact;
    if (effectRow.hitExtraControl.size() > 1 && effectRow.hitExtraControl[1] == 0 &&
        status::isGetUpStatus(ecs.get<RoleStatus>(target)) && hitRow.hitMust != 1)
        return false;
    ecs.get<RoleHitState>(target).m_beHitEffect = data.m_effect;
    buff::trigger(world, target, BFEvent::BeforeToBeHit, data);
    if (hitRow.hitType == -1)
        return true;
    const auto state = ecs.get<RoleStatus>(target);
    if (status::isDeathStatus(state) || status::isGetUpStatus(state) || status::isWakeStatus(state))
        return false;
    if (ecs.get<RoleIdentity>(target).roleType == static_cast<int>(EntityRoleType::kMachine))
    {
        auto& count = ecs.get<RoleHitState>(target).hitCounts;
        if (count == -1)
            return false;
        --count;
        if (isDead(world, target) && (!status::isTobeHitStatus(state) || status::isHitStatus(state)))
            status::dealWithStatus(world, target, EntityRoleStatus::Death);
        return true;
    }
    const bool floor = status::isHitFloorStatus(state) && ecs.get<Transform>(target).position.y == Fixed{};
    const bool air   = status::isHitUpStatus(state) || status::isHitDownStatus(state);
    if ((hitRow.hitCondition == 0 || hitRow.hitCondition == 2) && floor)
        return false;
    if ((hitRow.hitCondition == 1 || hitRow.hitCondition == 2) && air)
        return false;
    return true;
}
// 非霸体受击设置冻结、朝向与追击数据；状态叶子由独立模块执行。
void hitReaction(World& world, Entity target, const HitTrigger& data, const conf::SkillHitConfig& hitRow)
{
    auto& ecs           = world.ecs();
    const auto identity = ecs.get<RoleIdentity>(target);
    const auto* role    = conf::Config::instance().find<conf::RoleConfig>(identity.roleId);
    const auto deathId  = role ? role->deathDisplacementId : -1;
    if (ecs.get<ExtraState>(target).active(mugen::ExtraState::SuperArmor))
    {
        if (ecs.get<Transform>(target).position.y == Fixed{} && isDead(world, target))
            status::dealWithStatus(world, target, EntityRoleStatus::Death, deathId);
        else
            hitProtectInit(world, target);
        return;
    }
    const auto freeze = Fixed::fromInt(hitRow.freezeTime);
    if (hitRow.freezeTimeControlEffect == 0)
        ecs.get<EffectState>(data.m_effect).m_freezeMs = freeze;
    if (hitRow.freezeTimeControlRole == 0)
        ecs.get<RoleTimers>(data.m_attacker).freezeMs = freeze;
    auto& timers = ecs.singleton<LogicTimers>();
    timers.cancel(ecs.get<RoleHitState>(target).m_freezeTimer);
    ecs.get<RoleHitState>(target).m_freezeTimer =
        timers.add(target, Fixed::fromInt(hitRow.freezeTimeDelay), false, HitFreeze{freeze});
    const auto facing                  = -ecs.get<Transform>(data.m_effect).towardX;
    ecs.get<Transform>(target).towardX = facing;
    ecs.get<Transform>(target).vectorX = Fixed::fromInt(facing);
    const auto state                   = ecs.get<RoleStatus>(target);
    auto& reaction                     = ecs.get<RoleHitState>(target);
    const auto scale                   = "1"_fx + ecs.get<Attributes>(target).extend[15];
    if (reaction.stiffTime == Fixed{} || !status::isHitFloorStatus(state))
        reaction.stiffTime = Fixed::fromInt(hitRow.stiffTime) * scale;
    if (role && role->hitStiffTime != -1)
        reaction.stiffTime = Fixed::fromInt(role->hitStiffTime) * scale;
    if (status::isTobeHitStatus(state))
    {
        if ((identity.roleType != 1 && identity.roleType != 4) || !isDead(world, target))
        {
            auto& params  = ecs.get<RoleStatus>(target).entityParams;
            params.hit_id = hitRow.id;
            if (isRigidity(world, target))
                ++reaction.hitCounts;
            params.hit_counts = reaction.hitCounts;
        }
        updateHitProtectTime(world, target);
    }
    else if (isDead(world, target))
        status::dealWithStatus(world, target, EntityRoleStatus::Death, deathId);
    else
    {
        skill::forcedInterrupt(world, target);
        status::dealWithStatus(world, target, EntityRoleStatus::Hit, hitRow.id);
    }
}
// 非爆发状态按命中或受击回复能量。
void recoverEp(World& world, Entity entity, Fixed value)
{
    const auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (pool && pool->m_crazy)
        return;
    auto& vitals = world.ecs().get<Vitals>(entity);
    vitals.ep    = std::clamp(vitals.ep + value * vitals.m_epRecoveryScale, Fixed{}, vitals.maxEp);
}
// 命中结束先处理目标表现和能量，再处理攻击者输出和事件。
void endHit(World& world, Entity target, HitTrigger data, const conf::EffectConfig& row)
{
    auto& ecs             = world.ecs();
    const bool capability = ecs.get<EffectState>(data.m_effect).m_capabilityTrigger;
    if (!capability)
        world.hitHooks().m_ability(world, target, HitAbilityEvent::AfterToBeHit, data, -1);
    if (checkHurtPermit(world, target, data.m_effect, HurtPermit::Number))
        world.emit(target, HurtNumber{data.m_damage.toInt()});
    const auto owner = finalOwner(world, data.m_attacker);
    ai::addThreat(world, target, data.m_attacker, data.m_damage);
    if (const auto* combo = ecs.tryGet<RoleHitState>(owner))
        ecs.get<RoleHitState>(owner).m_comboExp += row.comboExp * combo->m_comboScale;
    const auto maxHp = ecs.get<Vitals>(target).maxHp;
    if (maxHp > Fixed{})
        recoverEp(world, target, std::min("100"_fx * data.m_damage / maxHp, "100"_fx) * conf::kCrazyFromHurtValue);
    if (!isDead(world, target))
        buff::trigger(world, target, BFEvent::AfterToBeHit, data);
    ecs.get<RoleHitState>(target).m_beHitEffect = {};
    auto& attack                                = ecs.get<RoleHitState>(data.m_attacker);
    attack.m_damage                             = data.m_damage;
    attack.m_critical                           = data.m_critical;
    attack.m_dps += data.m_damage;
    recoverEp(world, data.m_attacker, row.energy * (ecs.get<RoleIdentity>(target).roleType == 4 ? "1.5"_fx : "1"_fx));
    if (!capability)
        world.hitHooks().m_ability(world, data.m_attacker, HitAbilityEvent::AfterHit, data, -1);
    buff::trigger(world, data.m_attacker, BFEvent::AfterHit, data);
}
// 完整命中流程先占次数与目标，再依次进行主动闪避、属性闪避和伤害。
void hitTarget(World& world, Entity effectEntity, Entity target, const conf::EffectConfig& row, FixedVec2 contact)
{
    auto& ecs = world.ecs();
    if (!enabled(world, effectEntity) || row.hitId == -1)
        return;
    const auto* hitRow = conf::Config::instance().find<conf::SkillHitConfig>(row.hitId);
    if (!hitRow || !ecs.has<Vitals>(target))
        return;
    auto& source = ecs.get<EffectState>(effectEntity);
    if (source.m_hitInterval > Fixed{} || (hitRow->hitCounts != -1 && source.m_hitCount >= hitRow->hitCounts) ||
        isDead(world, target) || !ecs.get<Vitals>(target).m_beHitEnabled)
        return;
    if (hitRow->hitInterval == -1)
    {
        if (hitRow->hitCounts != -1 && static_cast<int64_t>(source.m_hitTargets.size()) >= hitRow->hitCounts)
            return;
        if (std::find(source.m_hitTargets.begin(), source.m_hitTargets.end(), target) != source.m_hitTargets.end())
            return;
        source.m_hitTargets.push_back(target);
    }
    ++source.m_hitCount;
    auto data = context(world, effectEntity, target);
    if (!row.hitExtraControl.empty() && row.hitExtraControl[0] == 0 && hitRow->hitMust != 1 &&
        ecs.get<ExtraState>(target).active(mugen::ExtraState::Invincible))
    {
        if (dodging(world, target))
            dodge(world, target, data, false);
        return;
    }
    // 普通命中在属性闪避之前按当前技能槽主动闪避。
    if (dodging(world, target))
    {
        dodge(world, target, data, false);
        return;
    }
    DamageInput input;
    if (!damageInput(world, effectEntity, target, input))
        return;
    if (attributeDodge(input, damageDraw(ecs.singleton<RngState>().random)))
    {
        dodge(world, target, data, true);
        return;
    }
    const bool accepted = beforeHit(world, target, data, row, *hitRow, contact);
    if (accepted && hitRow->hitType != -1)
    {
        buff::trigger(world, data.m_attacker, BFEvent::BeforeHit, data);
        if (!ecs.get<EffectState>(effectEntity).m_capabilityTrigger)
            world.hitHooks().m_ability(world, data.m_attacker, HitAbilityEvent::BeforeHit, data, -1);
        // 前置事件可以改变属性，伤害阶段重新解析而不重复抽闪避。
        if (!damageInput(world, effectEntity, target, input))
            return;
        const auto damage                    = calculateDamage(input, damageDraw(ecs.singleton<RngState>().random));
        data.m_damage                        = damage.m_damage;
        data.m_critical                      = damage.m_critical;
        ecs.get<RoleHitState>(target).m_hurt = data.m_damage;
        ecs.get<RoleHitState>(target).m_beCritical = data.m_critical;
        if (checkHurtPermit(world, target, effectEntity, HurtPermit::Hp) &&
            !world.hitHooks().m_replaceDamage(world, target, data.m_damage))
            modifyHp(world, target, -data.m_damage, effectEntity);
        hitReaction(world, target, data, *hitRow);
        world.hitHooks().m_ability(world, data.m_attacker, HitAbilityEvent::Anger, data, data.m_skillId);
        endHit(world, target, data, row);
    }
    ecs.get<RoleHitState>(target).m_hurt              = {};
    ecs.get<RoleHitState>(target).m_beCritical        = false;
    ecs.get<RoleHitState>(target).m_beHitEffect       = {};
    ecs.get<RoleHitState>(data.m_attacker).m_damage   = {};
    ecs.get<RoleHitState>(data.m_attacker).m_critical = false;
    if (accepted && enabled(world, effectEntity))
        hitEffects(world, effectEntity, target, contact);
}
// 按原始列表首项哨兵转交 Buff 施加；不提前消耗概率随机值。
void applyBuffs(World& world, Entity pool, const std::vector<Fixed>& ids, HitTrigger data)
{
    if (ids.empty() || ids.front() == "-1"_fx)
        return;
    data.m_target = {};
    for (const auto id : ids)
        buff::dealBuff(world, pool, {id, data});
}
// 整数配置列表转换为不丢精度的施加请求。
std::vector<Fixed> buffIds(const std::vector<int32_t>& ids)
{
    std::vector<Fixed> result;
    for (const auto id : ids)
        result.push_back(Fixed::fromInt(id));
    return result;
}
}  // namespace
// 碰撞分派按目标类型与攻击者阵营过滤，End 事件不产生打击。
void onEffectCollision(World& world, Entity effectEntity, Entity target, CollisionEvent event, FixedVec2 contact)
{
    auto& ecs = world.ecs();
    if (event == CollisionEvent::End || !enabled(world, target))
        return;
    const auto* effectState = ecs.tryGet<EffectState>(effectEntity);
    if (!effectState || !enabled(world, effectState->m_owner))
        return;
    const auto source = *effectState;
    const auto* row   = conf::Config::instance().find<conf::EffectConfig>(source.m_configId);
    if (!row)
        return;
    const auto kind = ecs.get<EntityGroup>(target).kind;
    auto data       = context(world, effectEntity, target);
    if (kind == GroupKind::Obstacle)
    {
        const auto camp = ecs.get<RoleIdentity>(source.m_owner).camp;
        if (camp != 2 && camp != 4)
            return;
        const auto* hitRow = conf::Config::instance().find<conf::SkillHitConfig>(row->hitId);
        auto* obstacle     = ecs.tryGet<ObstacleState>(target);
        if (!hitRow || !obstacle || obstacle->m_hits == 0 || obstacle->m_hits == -1)
            return;
        auto& state = ecs.get<EffectState>(effectEntity);
        if (state.m_hitInterval > Fixed{} || (hitRow->hitCounts != -1 && state.m_hitCount >= hitRow->hitCounts))
            return;
        if (hitRow->hitInterval == -1)
        {
            if (std::find(state.m_hitTargets.begin(), state.m_hitTargets.end(), target) != state.m_hitTargets.end())
                return;
            if (hitRow->hitCounts != -1 && state.m_hitTargets.size() >= static_cast<size_t>(hitRow->hitCounts))
                return;
            state.m_hitTargets.push_back(target);
        }
        ++state.m_hitCount;
        if (hitRow->hitType != -1)
            --obstacle->m_hits;
        obstacle->m_time        = {};
        auto& motion            = ecs.get<MotionState>(target);
        motion.motionName       = obstacle->m_hits == 0 ? "death" : "hit";
        motion.timeMs           = {};
        motion.m_completedLoops = 0;
        motion.loop             = false;
        hitEffects(world, effectEntity, target, contact);
        world.hitHooks().m_ability(world, target, HitAbilityEvent::Obstacle, data, row->hitId);
        return;
    }
    if (kind != GroupKind::Role)
        return;
    const auto identity = ecs.get<RoleIdentity>(target);
    if (identity.roleType == static_cast<int>(EntityRoleType::kCrystal))
    {
        if (!ecs.get<RoleHitState>(target).m_crystalHit)
        {
            ecs.get<RoleHitState>(target).m_crystalHit = true;
            world.hitHooks().m_ability(world, target, HitAbilityEvent::Crystal, data, -1);
            hitEffects(world, effectEntity, target, contact);
        }
        return;
    }
    const auto hostile = ecs.get<RoleIdentity>(source.m_owner).hostileCamp;
    if ((hostile & identity.camp) == identity.camp)
    {
        if (row->hitTarget == 1 || row->hitTarget == 2)
            return;
        hitTarget(world, effectEntity, target, *row, contact);
        if (!dodging(world, target))
            applyBuffs(world, target, buffIds(row->debuffId), data);
    }
    else
    {
        if (row->hitTarget == 0)
            return;
        if (row->hitTarget == 2)
            hitTarget(world, effectEntity, target, *row, contact);
        std::vector<Fixed> ids;
        if (target == source.m_owner)
            ids = row->buffId;
        else if (const auto* list = std::get_if<std::vector<int32_t>>(&row->buffAllId))
            ids = buffIds(*list);
        else
            ids = {Fixed::fromInt(std::get<int32_t>(row->buffAllId))};
        applyBuffs(world, source.m_owner, ids, data);
        data.m_target   = source.m_owner;
        data.m_teammate = true;
        buff::trigger(world, target, BFEvent::BeforeHit, data);
        buff::trigger(world, target, BFEvent::AfterHit, data);
        if (row->specialabilityId != -1)
        {
            world.hitHooks().m_ability(world, source.m_owner, HitAbilityEvent::SameCamp, data, row->specialabilityId);
            if (row->specialabilityAllId != -1)
            {
                data.m_target   = target;
                data.m_teammate = false;
                world.hitHooks().m_ability(world, target, HitAbilityEvent::SameCamp, data, row->specialabilityAllId);
            }
        }
    }
}
}  // namespace mugen::sim::hit
