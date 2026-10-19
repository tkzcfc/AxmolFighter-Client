#include "HitResolver.h"
#include "Damage.h"
#include "DamageCalculator.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/skill/SkillCaster.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/sim/trace/TraceSink.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::hit
{
HitResolver::HitResolver(World& world, Entity effect) : m_world(world), m_effect(effect) {}

// 碰撞分派按目标类型与攻击者阵营过滤，End 事件不产生打击。
void HitResolver::onCollision(Entity other, CollisionEvent event, FixedVec2 contact)
{
    if (event == CollisionEvent::End)
        return;
    m_target  = other;
    m_contact = contact;
    _resolve();
}

// 按配置目标选择接收池，实体遍历保持世界创建顺序。
void HitResolver::addEntityBuff(int32_t id, std::optional<int32_t> boundSkill, std::optional<int32_t> origin)
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(id);
    if (!config)
        return;
    if (auto* state = m_world.ecs().tryGet<RoleHitState>(m_effect))
        state->m_protectionBuffRequests.push_back(id);
    HitBuffApplication request;
    request.m_buffId = Fixed::fromInt(id);
    request.m_origin = origin;
    if (config->target == 1)
    {
        if (config->binding != 0)
            request.m_boundSkill = boundSkill;
        m_world.hitHooks().m_apply(m_world, m_effect, request);
        return;
    }
    const auto identity = m_world.ecs().get<RoleIdentity>(m_effect);
    m_world.ecs().each<RoleIdentity>([&](Entity target, const RoleIdentity& other) {
        const bool enemy    = (identity.hostileCamp & other.camp) != 0;
        const bool selected = config->target == 9 || (config->target == 4 && !enemy) ||
                              (config->target == 5 && enemy) || (config->target == 6 && other.roleType == 4) ||
                              (config->target == 7 && (other.roleType == 2 || other.roleType == 3));
        if (selected)
            m_world.hitHooks().m_apply(m_world, target, request);
    });
}

// 同步分派给世界实例的 Buff 接缝，允许在返回前执行免死。
void HitResolver::trigger(BFEvent event, const HitTrigger& context)
{
    m_world.hitHooks().m_trigger(m_world, m_effect, event, context);
}

// 转交施加请求，不在命中层建立 Buff 池。
void HitResolver::dealBuff(const HitBuffApplication& application)
{
    m_world.hitHooks().m_apply(m_world, m_effect, application);
}

bool HitResolver::_enabled(Entity entity) const
{
    const auto* group = m_world.ecs().tryGet<EntityGroup>(entity);
    return group && !group->destroyRequested;
}

bool HitResolver::_dodging(Entity entity) const
{
    const auto* pool = m_world.ecs().tryGet<SkillPool>(entity);
    return pool && pool->m_current != kNoSkill && pool->baseSlot(pool->m_skills.at(pool->m_current)) == "F";
}

HitTrigger HitResolver::_context(Entity effect, Entity target) const
{
    const auto source = m_world.ecs().get<EffectState>(effect);
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

void HitResolver::_dodge(Entity target, const HitTrigger& data, bool passive)
{
    HitResolver(m_world, target).trigger(passive ? BFEvent::AttackMiss : BFEvent::EvadeSuccess, data);
    m_world.emit(target, Tips{passive ? "MISS" : "DODGE"});
}

void HitResolver::_hitEffects(Entity sourceEntity, Entity target, FixedVec2 contact)
{
    auto& ecs         = m_world.ecs();
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
            if (child.value == 0 && state.m_configId == id && state.m_hitEntity == target && _enabled(entity))
                child = entity;
        });
        if (child.value == 0)
        {
            child = effect::EffectActor::create(m_world, source.m_owner, id);
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
        effect::EffectActor(m_world, child).enter();
    }
}

bool HitResolver::_beforeHit(Entity target,
                             const HitTrigger& data,
                             const conf::EffectConfig& effectRow,
                             const conf::SkillHitConfig& hitRow,
                             FixedVec2 contact)
{
    auto& ecs       = m_world.ecs();
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(ecs.get<RoleIdentity>(target).roleId);
    if (row && row->hurtNumPos.size() >= 2)
    {
        contact.x -= ecs.get<Transform>(data.m_effect).vectorX * Fixed::fromInt(row->hurtNumPos[0]);
        contact.y += Fixed::fromInt(row->hurtNumPos[1]);
    }
    ecs.get<RoleHitState>(target).m_contact = contact;
    if (effectRow.hitExtraControl.size() > 1 && effectRow.hitExtraControl[1] == 0 &&
        ecs.get<RoleStatus>(target).isGetUp() && hitRow.hitMust != 1)
        return false;
    ecs.get<RoleHitState>(target).m_beHitEffect = data.m_effect;
    HitResolver(m_world, target).trigger(BFEvent::BeforeToBeHit, data);
    if (hitRow.hitType == -1)
        return true;
    const auto state = ecs.get<RoleStatus>(target);
    if (state.isDeath() || state.isGetUp() || state.isWake())
        return false;
    if (ecs.get<RoleIdentity>(target).roleType == static_cast<int>(EntityRoleType::kMachine))
    {
        auto& count = ecs.get<RoleHitState>(target).hitCounts;
        if (count == -1)
            return false;
        --count;
        if (Role(m_world, target).isDead() && (!state.isTobeHit() || state.isHit()))
            Role(m_world, target).changeStatus(EntityRoleStatus::Death);
        return true;
    }
    const bool floor = state.isHitFloor() && ecs.get<Transform>(target).position.y == Fixed{};
    const bool air   = state.isHitUp() || state.isHitDown();
    if ((hitRow.hitCondition == 0 || hitRow.hitCondition == 2) && floor)
        return false;
    if ((hitRow.hitCondition == 1 || hitRow.hitCondition == 2) && air)
        return false;
    return true;
}

void HitResolver::_hitReaction(Entity target, const HitTrigger& data, const conf::SkillHitConfig& hitRow)
{
    auto& ecs           = m_world.ecs();
    const auto identity = ecs.get<RoleIdentity>(target);
    const auto* role    = conf::Config::instance().find<conf::RoleConfig>(identity.roleId);
    const auto deathId  = role ? role->deathDisplacementId : -1;
    if (ecs.get<ExtraState>(target).active(mugen::ExtraState::SuperArmor))
    {
        if (ecs.get<Transform>(target).position.y == Fixed{} && Role(m_world, target).isDead())
            Role(m_world, target).changeStatus(EntityRoleStatus::Death, deathId);
        else
            Role(m_world, target).initHitProtect();
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
    if (reaction.stiffTime == Fixed{} || !state.isHitFloor())
        reaction.stiffTime = Fixed::fromInt(hitRow.stiffTime) * scale;
    if (role && role->hitStiffTime != -1)
        reaction.stiffTime = Fixed::fromInt(role->hitStiffTime) * scale;
    if (state.isTobeHit())
    {
        if ((identity.roleType != 1 && identity.roleType != 4) || !Role(m_world, target).isDead())
        {
            auto& params  = ecs.get<RoleStatus>(target).entityParams;
            params.hit_id = hitRow.id;
            if (Role(m_world, target).isRigidity())
                ++reaction.hitCounts;
            params.hit_counts = reaction.hitCounts;
        }
        Role(m_world, target).refreshHitProtect();
    }
    else if (Role(m_world, target).isDead())
        Role(m_world, target).changeStatus(EntityRoleStatus::Death, deathId);
    else
    {
        skill::SkillCaster(m_world, target).forcedInterrupt();
        Role(m_world, target).changeStatus(EntityRoleStatus::Hit, hitRow.id);
    }
}

void HitResolver::_recoverEp(Entity entity, Fixed value)
{
    const auto* pool = m_world.ecs().tryGet<SkillPool>(entity);
    if (pool && pool->m_crazy)
        return;
    auto& vitals = m_world.ecs().get<Vitals>(entity);
    vitals.ep    = std::clamp(vitals.ep + value * vitals.m_epRecoveryScale, Fixed{}, vitals.maxEp);
}

void HitResolver::_endHit(Entity target, HitTrigger data, const conf::EffectConfig& row)
{
    auto& ecs             = m_world.ecs();
    const bool capability = ecs.get<EffectState>(data.m_effect).m_capabilityTrigger;
    if (!capability)
        m_world.hitHooks().m_ability(m_world, target, HitAbilityEvent::AfterToBeHit, data, -1);
    if (DamageCalculator(m_world).checkHurtPermit(target, data.m_effect, HurtPermit::Number))
        m_world.emit(target, HurtNumber{data.m_damage.toInt()});
    const auto owner = DamageCalculator(m_world).finalOwner(data.m_attacker);
    ai::AiDriver(m_world, target).addThreat(data.m_attacker, data.m_damage);
    if (const auto* combo = ecs.tryGet<RoleHitState>(owner))
        ecs.get<RoleHitState>(owner).m_comboExp += row.comboExp * combo->m_comboScale;
    const auto maxHp = ecs.get<Vitals>(target).maxHp;
    if (maxHp > Fixed{})
        _recoverEp(target, std::min("100"_fx * data.m_damage / maxHp, "100"_fx) * conf::kCrazyFromHurtValue);
    if (!Role(m_world, target).isDead())
        HitResolver(m_world, target).trigger(BFEvent::AfterToBeHit, data);
    ecs.get<RoleHitState>(target).m_beHitEffect = {};
    auto& attack                                = ecs.get<RoleHitState>(data.m_attacker);
    attack.m_damage                             = data.m_damage;
    attack.m_critical                           = data.m_critical;
    attack.m_dps += data.m_damage;
    _recoverEp(data.m_attacker, row.energy * (ecs.get<RoleIdentity>(target).roleType == 4 ? "1.5"_fx : "1"_fx));
    if (!capability)
        m_world.hitHooks().m_ability(m_world, data.m_attacker, HitAbilityEvent::AfterHit, data, -1);
    HitResolver(m_world, data.m_attacker).trigger(BFEvent::AfterHit, data);
}

void HitResolver::_applyBuffs(Entity pool, const std::vector<Fixed>& ids, HitTrigger data)
{
    if (ids.empty() || ids.front() == "-1"_fx)
        return;
    data.m_target = {};
    for (const auto id : ids)
        HitResolver(m_world, pool).dealBuff({id, data});
}

std::vector<Fixed> HitResolver::_buffIds(const std::vector<int32_t>& ids)
{
    std::vector<Fixed> result;
    for (const auto id : ids)
        result.push_back(Fixed::fromInt(id));
    return result;
}

void HitResolver::_hitTarget(const conf::EffectConfig& row)
{
    auto& ecs = m_world.ecs();
    if (!_enabled(m_effect) || row.hitId == -1)
        return;
    const auto* hitRow = conf::Config::instance().find<conf::SkillHitConfig>(row.hitId);
    if (!hitRow || !ecs.has<Vitals>(m_target))
        return;
    auto& source = ecs.get<EffectState>(m_effect);
    if (source.m_hitInterval > Fixed{} || (hitRow->hitCounts != -1 && source.m_hitCount >= hitRow->hitCounts) ||
        Role(m_world, m_target).isDead() || !ecs.get<Vitals>(m_target).m_beHitEnabled)
        return;
    if (hitRow->hitInterval == -1)
    {
        if (hitRow->hitCounts != -1 && static_cast<int64_t>(source.m_hitTargets.size()) >= hitRow->hitCounts)
            return;
        if (std::find(source.m_hitTargets.begin(), source.m_hitTargets.end(), m_target) != source.m_hitTargets.end())
            return;
        source.m_hitTargets.push_back(m_target);
    }
    ++source.m_hitCount;
    auto data = _context(m_effect, m_target);
    if (!row.hitExtraControl.empty() && row.hitExtraControl[0] == 0 && hitRow->hitMust != 1 &&
        ecs.get<ExtraState>(m_target).active(mugen::ExtraState::Invincible))
    {
        if (_dodging(m_target))
            _dodge(m_target, data, false);
        return;
    }
    // 普通命中在属性闪避之前按当前技能槽主动闪避。
    if (_dodging(m_target))
    {
        _dodge(m_target, data, false);
        return;
    }
    DamageInput input;
    if (!DamageCalculator(m_world).damageInput(m_effect, m_target, input))
        return;
    if (attributeDodge(input, damageDraw(ecs.singleton<RngState>().random)))
    {
        _dodge(m_target, data, true);
        return;
    }
    const bool accepted = _beforeHit(m_target, data, row, *hitRow, m_contact);
    if (accepted && hitRow->hitType != -1)
    {
        HitResolver(m_world, data.m_attacker).trigger(BFEvent::BeforeHit, data);
        if (!ecs.get<EffectState>(m_effect).m_capabilityTrigger)
            m_world.hitHooks().m_ability(m_world, data.m_attacker, HitAbilityEvent::BeforeHit, data, -1);
        // 前置事件可以改变属性，伤害阶段重新解析而不重复抽闪避。
        if (!DamageCalculator(m_world).damageInput(m_effect, m_target, input))
            return;
        const auto damage                      = calculateDamage(input, damageDraw(ecs.singleton<RngState>().random));
        data.m_damage                          = damage.m_damage;
        data.m_critical                        = damage.m_critical;
        ecs.get<RoleHitState>(m_target).m_hurt = data.m_damage;
        ecs.get<RoleHitState>(m_target).m_beCritical = data.m_critical;
        if (DamageCalculator(m_world).checkHurtPermit(m_target, m_effect, HurtPermit::Hp) &&
            !m_world.hitHooks().m_replaceDamage(m_world, m_target, data.m_damage))
        {
            const int32_t sourceConfig = ecs.get<EffectState>(m_effect).m_configId;
            const auto applied         = data.m_damage.toInt();
            const bool critical        = data.m_critical;
            const auto attacker        = data.m_attacker;
            trace::TraceRecorder(m_world).noteDamage(attacker, m_target, sourceConfig, applied, critical);
            Role(m_world, m_target).modifyHp(-data.m_damage, m_effect);
        }
        _hitReaction(m_target, data, *hitRow);
        m_world.hitHooks().m_ability(m_world, data.m_attacker, HitAbilityEvent::Anger, data, data.m_skillId);
        _endHit(m_target, data, row);
    }
    ecs.get<RoleHitState>(m_target).m_hurt            = {};
    ecs.get<RoleHitState>(m_target).m_beCritical      = false;
    ecs.get<RoleHitState>(m_target).m_beHitEffect     = {};
    ecs.get<RoleHitState>(data.m_attacker).m_damage   = {};
    ecs.get<RoleHitState>(data.m_attacker).m_critical = false;
    if (accepted && _enabled(m_effect))
        _hitEffects(m_effect, m_target, m_contact);
}

void HitResolver::_resolve()
{
    auto& ecs = m_world.ecs();
    if (!_enabled(m_target))
        return;
    const auto* effectState = ecs.tryGet<EffectState>(m_effect);
    if (!effectState || !_enabled(effectState->m_owner))
        return;
    const auto source = *effectState;
    const auto* row   = conf::Config::instance().find<conf::EffectConfig>(source.m_configId);
    if (!row)
        return;
    const auto kind = ecs.get<EntityGroup>(m_target).kind;
    HitTrigger data = _context(m_effect, m_target);
    if (kind == GroupKind::Obstacle)
    {
        const auto camp = ecs.get<RoleIdentity>(source.m_owner).camp;
        if (camp != 2 && camp != 4)
            return;
        const auto* hitRow = conf::Config::instance().find<conf::SkillHitConfig>(row->hitId);
        auto* obstacle     = ecs.tryGet<ObstacleState>(m_target);
        if (!hitRow || !obstacle || obstacle->m_hits == 0 || obstacle->m_hits == -1)
            return;
        auto& state = ecs.get<EffectState>(m_effect);
        if (state.m_hitInterval > Fixed{} || (hitRow->hitCounts != -1 && state.m_hitCount >= hitRow->hitCounts))
            return;
        if (hitRow->hitInterval == -1)
        {
            if (std::find(state.m_hitTargets.begin(), state.m_hitTargets.end(), m_target) != state.m_hitTargets.end())
                return;
            if (hitRow->hitCounts != -1 && state.m_hitTargets.size() >= static_cast<size_t>(hitRow->hitCounts))
                return;
            state.m_hitTargets.push_back(m_target);
        }
        ++state.m_hitCount;
        if (hitRow->hitType != -1)
            --obstacle->m_hits;
        obstacle->m_time        = {};
        auto& motion            = ecs.get<MotionState>(m_target);
        motion.motionName       = obstacle->m_hits == 0 ? "death" : "hit";
        motion.timeMs           = {};
        motion.m_completedLoops = 0;
        motion.loop             = false;
        _hitEffects(m_effect, m_target, m_contact);
        m_world.hitHooks().m_ability(m_world, m_target, HitAbilityEvent::Obstacle, data, row->hitId);
        return;
    }
    if (kind != GroupKind::Role)
        return;
    const auto identity = ecs.get<RoleIdentity>(m_target);
    if (identity.roleType == static_cast<int>(EntityRoleType::kCrystal))
    {
        if (!ecs.get<RoleHitState>(m_target).m_crystalHit)
        {
            ecs.get<RoleHitState>(m_target).m_crystalHit = true;
            m_world.hitHooks().m_ability(m_world, m_target, HitAbilityEvent::Crystal, data, -1);
            _hitEffects(m_effect, m_target, m_contact);
        }
        return;
    }
    const auto hostile = ecs.get<RoleIdentity>(source.m_owner).hostileCamp;
    if ((hostile & identity.camp) == identity.camp)
    {
        if (row->hitTarget == 1 || row->hitTarget == 2)
            return;
        _hitTarget(*row);
        if (!_dodging(m_target))
            _applyBuffs(m_target, _buffIds(row->debuffId), data);
    }
    else
    {
        if (row->hitTarget == 0)
            return;
        if (row->hitTarget == 2)
            _hitTarget(*row);
        std::vector<Fixed> ids;
        if (m_target == source.m_owner)
            ids = row->buffId;
        else if (const auto* list = std::get_if<std::vector<int32_t>>(&row->buffAllId))
            ids = _buffIds(*list);
        else
            ids = {Fixed::fromInt(std::get<int32_t>(row->buffAllId))};
        _applyBuffs(source.m_owner, ids, data);
        data.m_target   = source.m_owner;
        data.m_teammate = true;
        HitResolver(m_world, m_target).trigger(BFEvent::BeforeHit, data);
        HitResolver(m_world, m_target).trigger(BFEvent::AfterHit, data);
        if (row->specialabilityId != -1)
        {
            m_world.hitHooks().m_ability(m_world, source.m_owner, HitAbilityEvent::SameCamp, data,
                                         row->specialabilityId);
            if (row->specialabilityAllId != -1)
            {
                data.m_target   = m_target;
                data.m_teammate = false;
                m_world.hitHooks().m_ability(m_world, m_target, HitAbilityEvent::SameCamp, data,
                                             row->specialabilityAllId);
            }
        }
    }
}
}  // namespace mugen::sim::hit
