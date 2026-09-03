#include "mugen/buff/BuffRules.h"

#include "mugen/buff/Buff.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/skill/Skill.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{
float param0(Entity* /*entity*/, const Buff& inst)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    return BuffRuleUtil::param(cfg, 0, 0.0f);
}

void applyExtendDelta(Entity* entity, Buff& inst, ExtendAttributeType type, float target)
{
    const float delta = target - inst.appliedValue;
    if (delta != 0.0f)
        BuffRuleUtil::modifyExtend(entity, type, delta);
    inst.appliedValue = target;
    inst.applied      = (target != 0.0f);
}

void clearExtend(Entity* entity, Buff& inst, ExtendAttributeType type)
{
    if (inst.appliedValue != 0.0f)
        BuffRuleUtil::modifyExtend(entity, type, -inst.appliedValue);
    inst.appliedValue = 0.0f;
    inst.applied      = false;
}
}  // namespace

void BuffRuleInvincible::onAdd(Entity* entity, Buff& /*inst*/)
{
    if (auto* mgr = BuffManager::of(entity))
        ++mgr->invincibleRef;
}

void BuffRuleInvincible::onRemove(Entity* entity, Buff& /*inst*/)
{
    if (auto* mgr = BuffManager::of(entity))
        mgr->invincibleRef = (std::max)(0, mgr->invincibleRef - 1);
}

void BuffRuleInvincible::onStack(Entity* /*entity*/, Buff& /*inst*/) {}

void BuffRuleSuperArmor::onAdd(Entity* entity, Buff& /*inst*/)
{
    if (auto* mgr = BuffManager::of(entity))
        ++mgr->superArmorRef;
}

void BuffRuleSuperArmor::onRemove(Entity* entity, Buff& /*inst*/)
{
    if (auto* mgr = BuffManager::of(entity))
        mgr->superArmorRef = (std::max)(0, mgr->superArmorRef - 1);
}

void BuffRuleSuperArmor::onStack(Entity* /*entity*/, Buff& /*inst*/) {}

bool BuffRulePeriodicHurt::onTick(Entity* entity, Buff& inst, int32_t dtMs)
{
    const auto* cfg        = Config::getInstance()->getBuffConfigById(inst.buffId);
    const int32_t interval = BuffRuleUtil::intervalMs(cfg);
    if (!cfg || interval <= 0)
        return true;

    inst.tickAccumMs += dtMs;
    if (inst.tickAccumMs >= interval)
    {
        inst.tickAccumMs     = 0;
        const float delta    = cfg->paramValue.empty() ? 0.0f : cfg->paramValue[0];
        const int32_t stacks = (std::max)(1, inst.repeatCount);
        BuffRuleUtil::applyHpDelta(entity, delta * static_cast<float>(stacks));
    }
    return true;
}

void BuffRuleDamageHurt::onAdd(Entity* entity, Buff& inst)
{
    const float v = param0(entity, inst) * static_cast<float>((std::max)(1, inst.repeatCount));
    applyExtendDelta(entity, inst, ExtendAttributeType::AddHurt, v);
}

void BuffRuleDamageHurt::onRemove(Entity* entity, Buff& inst)
{
    clearExtend(entity, inst, ExtendAttributeType::AddHurt);
}

void BuffRuleDamageHurt::onBegin(Entity* entity,
                                 Buff& inst,
                                 BFEvent /*event*/,
                                 Entity* /*other*/,
                                 int32_t skillId,
                                 float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    if (inst.applied)
        return;
    onAdd(entity, inst);
}

void BuffRuleDamageHurt::onEnd(Entity* entity,
                               Buff& inst,
                               BFEvent /*event*/,
                               Entity* /*other*/,
                               int32_t /*skillId*/,
                               float /*param*/)
{
    if (inst.applied)
        onRemove(entity, inst);
}

void BuffRuleDamageReduction::onAdd(Entity* entity, Buff& inst)
{
    applyExtendDelta(entity, inst, ExtendAttributeType::AvoidHurt, param0(entity, inst));
}

void BuffRuleDamageReduction::onRemove(Entity* entity, Buff& inst)
{
    clearExtend(entity, inst, ExtendAttributeType::AvoidHurt);
}

void BuffRuleDamageReduction::onBegin(Entity* entity,
                                      Buff& inst,
                                      BFEvent /*event*/,
                                      Entity* /*other*/,
                                      int32_t skillId,
                                      float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    if (!inst.applied)
        onAdd(entity, inst);
}

void BuffRuleDamageReduction::onEnd(Entity* entity,
                                    Buff& inst,
                                    BFEvent /*event*/,
                                    Entity* /*other*/,
                                    int32_t /*skillId*/,
                                    float /*param*/)
{
    if (inst.applied)
        onRemove(entity, inst);
}

void BuffRuleDamageSlot::onAdd(Entity* entity, Buff& inst)
{
    // 预计算：每满段槽 * param[0]；简化为当前牌组技能数 * param（无满段信息时用 1）
    auto* mgr         = SkillManager::of(entity);
    int32_t fullSlots = 0;
    if (mgr)
    {
        for (const auto& s : mgr->skills)
        {
            if (s && s->releaseMax > 0 && s->releaseCount <= 0)
                ++fullSlots;
        }
    }
    if (fullSlots <= 0)
        fullSlots = 1;
    inst.appliedValue = param0(entity, inst) * static_cast<float>(fullSlots);
    // 实际 ADD_HURT 在 onBegin 挂上
}

void BuffRuleDamageSlot::onRemove(Entity* entity, Buff& inst)
{
    if (inst.applied)
    {
        BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddHurt, -inst.appliedValue);
        inst.applied = false;
    }
    inst.appliedValue = 0.0f;
}

void BuffRuleDamageSlot::onBegin(Entity* entity,
                                 Buff& inst,
                                 BFEvent /*event*/,
                                 Entity* /*other*/,
                                 int32_t skillId,
                                 float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    if (inst.applied || inst.appliedValue == 0.0f)
        return;
    // 仅当施法技能属于牌组时触发
    if (skillId > 0 && !BuffRuleUtil::findSkill(entity, skillId))
        return;
    BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddHurt, inst.appliedValue);
    inst.applied = true;
}

void BuffRuleDamageSlot::onEnd(Entity* entity,
                               Buff& inst,
                               BFEvent /*event*/,
                               Entity* /*other*/,
                               int32_t /*skillId*/,
                               float /*param*/)
{
    if (!inst.applied)
        return;
    BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddHurt, -inst.appliedValue);
    inst.applied = false;
}

void BuffRuleCDSkill::onAdd(Entity* entity, Buff& inst)
{
    const float scale = param0(entity, inst);
    auto* mgr         = SkillManager::of(entity);
    if (!mgr)
        return;
    const int32_t bindId = inst.sourceSkillId;
    for (auto& s : mgr->skills)
    {
        if (!s)
            continue;
        if (bindId > 0 && s->skillAttackId != bindId)
            continue;
        s->coldTimeScale += scale;
    }
    inst.appliedValue = scale;
    inst.applied      = true;
}

void BuffRuleCDSkill::onRemove(Entity* entity, Buff& inst)
{
    if (!inst.applied)
        return;
    auto* mgr = SkillManager::of(entity);
    if (!mgr)
        return;
    const int32_t bindId = inst.sourceSkillId;
    for (auto& s : mgr->skills)
    {
        if (!s)
            continue;
        if (bindId > 0 && s->skillAttackId != bindId)
            continue;
        s->coldTimeScale -= inst.appliedValue;
        if (s->coldTimeScale < 0.01f)
            s->coldTimeScale = 0.01f;
    }
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleModifyCDSkill::onAdd(Entity* entity, Buff& inst)
{
    const float scale = param0(entity, inst);
    auto* mgr         = SkillManager::of(entity);
    if (!mgr)
        return;
    const int32_t bindId = inst.sourceSkillId;
    for (auto& s : mgr->skills)
    {
        if (!s)
            continue;
        if (bindId > 0 && s->skillAttackId != bindId)
            continue;
        const int32_t delta = static_cast<int32_t>(static_cast<float>(s->coolDownMaxMs) * scale);
        s->coolDownMs       = (std::max)(0, s->coolDownMs + delta);
    }
}

void BuffRuleModifyCDSkill::onBegin(Entity* entity,
                                    Buff& inst,
                                    BFEvent /*event*/,
                                    Entity* /*other*/,
                                    int32_t skillId,
                                    float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    onAdd(entity, inst);
}

void BuffRuleTPConsumeScale::onAdd(Entity* entity, Buff& inst)
{
    const float scale    = param0(entity, inst);
    const int32_t bindId = inst.sourceSkillId;
    if (bindId > 0)
    {
        if (auto* sk = BuffRuleUtil::findSkill(entity, bindId))
            sk->mpConsumeScale += scale;
    }
    else if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        attr->mpConsumeScale += scale;
    }
    inst.appliedValue = scale;
    inst.applied      = true;
}

void BuffRuleTPConsumeScale::onRemove(Entity* entity, Buff& inst)
{
    if (!inst.applied)
        return;
    const int32_t bindId = inst.sourceSkillId;
    if (bindId > 0)
    {
        if (auto* sk = BuffRuleUtil::findSkill(entity, bindId))
            sk->mpConsumeScale -= inst.appliedValue;
    }
    else if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        attr->mpConsumeScale -= inst.appliedValue;
    }
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleEPConsumeScale::onAdd(Entity* entity, Buff& inst)
{
    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        const float scale = param0(entity, inst);
        attr->epConsumeScale += scale;
        inst.appliedValue = scale;
        inst.applied      = true;
    }
}

void BuffRuleEPConsumeScale::onRemove(Entity* entity, Buff& inst)
{
    if (!inst.applied)
        return;
    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        attr->epConsumeScale -= inst.appliedValue;
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleCrit::onAdd(Entity* entity, Buff& inst)
{
    applyExtendDelta(entity, inst, ExtendAttributeType::AddCrit, param0(entity, inst));
}

void BuffRuleCrit::onRemove(Entity* entity, Buff& inst)
{
    clearExtend(entity, inst, ExtendAttributeType::AddCrit);
}

void BuffRuleHPMAX::onAdd(Entity* entity, Buff& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    const float rate    = param0(entity, inst);
    const float baseMax = attr->basic.hpMax > 0.0f ? attr->basic.hpMax : attr->hp;
    const float add     = baseMax * rate;
    BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddMaxHp, add);
    attr->basic.hpMax = baseMax + add;
    attr->hp += attr->hp * rate;
    inst.appliedValue = add;
    inst.applied      = true;
}

void BuffRuleHPMAX::onRemove(Entity* entity, Buff& inst)
{
    if (!inst.applied)
        return;
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (attr)
    {
        BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddMaxHp, -inst.appliedValue);
        attr->basic.hpMax = (std::max)(1.0f, attr->basic.hpMax - inst.appliedValue);
        if (attr->hp > attr->basic.hpMax)
            attr->hp = attr->basic.hpMax;
    }
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleHP::onAdd(Entity* entity, Buff& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    const float v     = param0(entity, inst);
    const float delta = v > 1.0f ? v : attr->basic.hpMax * v;
    BuffRuleUtil::applyHpDelta(entity, delta);
}

bool BuffRuleHP::onTick(Entity* entity, Buff& inst, int32_t dtMs)
{
    const auto* cfg        = Config::getInstance()->getBuffConfigById(inst.buffId);
    const int32_t interval = BuffRuleUtil::intervalMs(cfg);
    if (!cfg || interval <= 0)
        return true;
    inst.tickAccumMs += dtMs;
    if (inst.tickAccumMs >= interval)
    {
        inst.tickAccumMs = 0;
        onAdd(entity, inst);
    }
    return true;
}

void BuffRuleStun::onAdd(Entity* entity, Buff& /*inst*/)
{
    auto* mgr = BuffManager::of(entity);
    auto* beh = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (mgr)
        ++mgr->stunRef;
    if (beh)
    {
        const int32_t oldKind = beh->currentKind;
        beh->statusTags |= StateTag::kTagHitState;
        beh->statusTags &= ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed);
        beh->currentKind = static_cast<int32_t>(BehaviorKind::kStun);
        BuffRuleUtil::notifyBehaviorKindChange(entity, oldKind, beh->currentKind);
    }
    if (auto* skillMgr = SkillManager::of(entity))
        skillMgr->forceInterruptCast(entity);
}

void BuffRuleStun::onRemove(Entity* entity, Buff& /*inst*/)
{
    auto* mgr = BuffManager::of(entity);
    auto* beh = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (mgr)
        mgr->stunRef = (std::max)(0, mgr->stunRef - 1);
    if (beh && (!mgr || mgr->stunRef <= 0) && !(beh->statusTags & StateTag::kTagDownState))
    {
        beh->statusTags &= ~StateTag::kTagHitState;
        beh->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
    }
}

void BuffRuleSpeed::onAdd(Entity* entity, Buff& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    const float scale = param0(entity, inst);
    const float delta = attr->moveSpeed * scale;
    attr->moveSpeed += delta;
    inst.appliedValue = delta;
    inst.applied      = true;
}

void BuffRuleSpeed::onRemove(Entity* entity, Buff& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (attr && inst.appliedValue != 0.0f)
        attr->moveSpeed -= inst.appliedValue;
    inst.appliedValue = 0.0f;
    inst.applied      = false;
}

void BuffRuleSpeed::onStack(Entity* entity, Buff& inst)
{
    onRemove(entity, inst);
    onAdd(entity, inst);
}

void BuffRuleCrazy::onAdd(Entity* entity, Buff& inst)
{
    auto* mgr = SkillManager::of(entity);
    if (!mgr)
        return;
    mgr->startCrazy(entity);
}

void BuffRuleCrazy::onRemove(Entity* entity, Buff& /*inst*/)
{
    auto* mgr = SkillManager::of(entity);
    if (!mgr)
        return;
    mgr->endCrazy(entity);
}

bool BuffRuleHPLock::onTick(Entity* entity, Buff& inst, int32_t /*dtMs*/)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return true;
    const float floorHp = param0(entity, inst);
    const float minHp   = floorHp > 1.0f ? floorHp : 1.0f;
    if (attr->hp < minHp)
        attr->hp = minHp;
    return true;
}

void BuffRuleDurance::onAdd(Entity* entity, Buff& inst)
{
    auto* beh = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!beh)
        return;
    beh->staticRemainMs = (std::max)(beh->staticRemainMs, inst.remainingMs);
}

void BuffRuleSneer::onAdd(Entity* /*entity*/, Buff& /*inst*/) {}

namespace
{
void applicatorApply(Entity* entity, Buff& inst, Entity* other)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg)
        return;
    for (float raw : cfg->paramValue)
    {
        const int32_t childId = static_cast<int32_t>(raw);
        if (childId <= 0 || childId == inst.buffId)
            continue;
        BuffRuleUtil::addBuffToTarget(entity, other, childId, inst.sourceSkillId);
    }
    inst.applied = true;
}

void applicatorRemove(Entity* entity, Buff& inst)
{
    if (!inst.applied)
        return;
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg)
        return;
    auto* mgr = BuffManager::of(entity);
    if (!mgr)
    {
        inst.applied = false;
        return;
    }
    for (float raw : cfg->paramValue)
    {
        const int32_t childId = static_cast<int32_t>(raw);
        if (childId > 0 && childId != inst.buffId)
            mgr->removeBuff(entity, childId);
    }
    inst.applied = false;
}

void addByStateApplyChildren(Entity* entity, Buff& inst)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg)
        return;
    for (size_t i = 2; i < cfg->paramValue.size(); ++i)
    {
        const int32_t childId = static_cast<int32_t>(cfg->paramValue[i]);
        if (childId <= 0 || childId == inst.buffId)
            continue;
        BuffRuleUtil::addBuffToTarget(entity, nullptr, childId, inst.sourceSkillId);
    }
    inst.applied = true;
}

void addByStateRemoveChildren(Entity* entity, Buff& inst)
{
    if (!inst.applied)
        return;
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    auto* mgr       = BuffManager::of(entity);
    if (cfg && mgr)
    {
        for (size_t i = 2; i < cfg->paramValue.size(); ++i)
        {
            const int32_t childId = static_cast<int32_t>(cfg->paramValue[i]);
            if (childId > 0 && childId != inst.buffId)
                mgr->removeBuff(entity, childId);
        }
    }
    inst.applied = false;
}
}  // namespace

void BuffRuleAddByApplicator::onAdd(Entity* entity, Buff& inst)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg || cfg->executeType > 1)
        return;
    applicatorApply(entity, inst, nullptr);
}

void BuffRuleAddByApplicator::onRemove(Entity* entity, Buff& inst)
{
    applicatorRemove(entity, inst);
}

void BuffRuleAddByApplicator::onBegin(Entity* entity,
                                      Buff& inst,
                                      BFEvent /*event*/,
                                      Entity* other,
                                      int32_t /*skillId*/,
                                      float /*param*/)
{
    applicatorApply(entity, inst, other);
}

void BuffRuleAddByApplicator::onEnd(Entity* entity,
                                    Buff& inst,
                                    BFEvent /*event*/,
                                    Entity* /*other*/,
                                    int32_t /*skillId*/,
                                    float /*param*/)
{
    applicatorRemove(entity, inst);
}

void BuffRuleAddByState::onAdd(Entity* /*entity*/, Buff& inst)
{
    inst.applied     = false;
    inst.stateValid  = false;
    inst.tickAccumMs = 0;
}

void BuffRuleAddByState::onRemove(Entity* entity, Buff& inst)
{
    addByStateRemoveChildren(entity, inst);
}

bool BuffRuleAddByState::onTick(Entity* entity, Buff& inst, int32_t dtMs)
{
    if (!inst.stateValid || inst.applied)
        return true;
    const auto* cfg     = Config::getInstance()->getBuffConfigById(inst.buffId);
    const float maxTime = BuffRuleUtil::param(cfg, 1, 0.0f);
    int32_t maxMs       = 0;
    if (maxTime > 0.0f)
        maxMs = maxTime < 100.0f ? static_cast<int32_t>(maxTime * 1000.0f) : static_cast<int32_t>(maxTime);
    inst.tickAccumMs += dtMs;
    if (inst.tickAccumMs >= maxMs)
    {
        inst.stateValid  = false;
        inst.tickAccumMs = 0;
        addByStateApplyChildren(entity, inst);
    }
    return true;
}

void BuffRuleAddByState::onEvent(Entity* /*entity*/,
                                 Buff& inst,
                                 BFEvent event,
                                 Entity* /*other*/,
                                 int32_t skillId,
                                 float /*param*/)
{
    const auto* cfg    = Config::getInstance()->getBuffConfigById(inst.buffId);
    const int32_t want = static_cast<int32_t>(BuffRuleUtil::param(cfg, 0, 0.0f));
    if (want <= 0 || skillId != want)
        return;
    if (event == BFEvent::BehaviorStateStart)
    {
        inst.stateValid  = true;
        inst.tickAccumMs = 0;
    }
    else if (event == BFEvent::BehaviorStateEnd)
    {
        inst.stateValid = false;
    }
}

void BuffRuleTP::onAdd(Entity* entity, Buff& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    const float v = param0(entity, inst);
    if (v > 1.0f)
        BuffRuleUtil::modifyMp(entity, v);
    else
        BuffRuleUtil::modifyMp(entity, attr->mpMax * v);
}

void BuffRuleTP::onBegin(Entity* entity,
                         Buff& inst,
                         BFEvent /*event*/,
                         Entity* /*other*/,
                         int32_t /*skillId*/,
                         float /*param*/)
{
    onAdd(entity, inst);
}

NS_MG_END
