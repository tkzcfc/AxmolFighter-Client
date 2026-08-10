#include "mugen/buff/BuffRules.h"

#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/component/BuffComponent.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{
float param0(Entity* /*entity*/, const BuffInstance& inst)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    return BuffRuleUtil::param(cfg, 0, 0.0f);
}

void applyExtendDelta(Entity* entity, BuffInstance& inst, ExtendAttributeType type, float target)
{
    const float delta = target - inst.appliedValue;
    if (delta != 0.0f)
        BuffRuleUtil::modifyExtend(entity, type, delta);
    inst.appliedValue = target;
    inst.applied      = (target != 0.0f);
}

void clearExtend(Entity* entity, BuffInstance& inst, ExtendAttributeType type)
{
    if (inst.appliedValue != 0.0f)
        BuffRuleUtil::modifyExtend(entity, type, -inst.appliedValue);
    inst.appliedValue = 0.0f;
    inst.applied      = false;
}
}  // namespace

void BuffRuleInvincible::onAdd(Entity* entity, BuffInstance& /*inst*/)
{
    if (auto* buff = MG_GET_COMPONENT(entity, BuffComponent))
        ++buff->invincibleRef;
}

void BuffRuleInvincible::onRemove(Entity* entity, BuffInstance& /*inst*/)
{
    if (auto* buff = MG_GET_COMPONENT(entity, BuffComponent))
        buff->invincibleRef = (std::max)(0, buff->invincibleRef - 1);
}

void BuffRuleInvincible::onStack(Entity* /*entity*/, BuffInstance& /*inst*/) {}

void BuffRuleSuperArmor::onAdd(Entity* entity, BuffInstance& /*inst*/)
{
    if (auto* buff = MG_GET_COMPONENT(entity, BuffComponent))
        ++buff->superArmorRef;
}

void BuffRuleSuperArmor::onRemove(Entity* entity, BuffInstance& /*inst*/)
{
    if (auto* buff = MG_GET_COMPONENT(entity, BuffComponent))
        buff->superArmorRef = (std::max)(0, buff->superArmorRef - 1);
}

void BuffRuleSuperArmor::onStack(Entity* /*entity*/, BuffInstance& /*inst*/) {}

bool BuffRulePeriodicHurt::onTick(Entity* entity, BuffInstance& inst, int32_t dtMs)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!attr || !cfg || cfg->interval <= 0)
        return true;

    inst.tickAccumMs += dtMs;
    if (inst.tickAccumMs >= cfg->interval)
    {
        inst.tickAccumMs = 0;
        const float delta = cfg->paramValue.empty() ? 0.0f : cfg->paramValue[0];
        const int32_t stacks = (std::max)(1, inst.repeatCount);
        attr->currentAttribute.hp =
            (std::max)(0.0f, attr->currentAttribute.hp + delta * static_cast<float>(stacks));
    }
    return true;
}

void BuffRuleDamageHurt::onAdd(Entity* entity, BuffInstance& inst)
{
    const float v = param0(entity, inst) * static_cast<float>((std::max)(1, inst.repeatCount));
    applyExtendDelta(entity, inst, ExtendAttributeType::AddHurt, v);
}

void BuffRuleDamageHurt::onRemove(Entity* entity, BuffInstance& inst)
{
    clearExtend(entity, inst, ExtendAttributeType::AddHurt);
}

void BuffRuleDamageHurt::onBegin(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                                 int32_t skillId, float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    if (inst.applied)
        return;
    onAdd(entity, inst);
}

void BuffRuleDamageHurt::onEnd(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                               int32_t /*skillId*/, float /*param*/)
{
    if (inst.applied)
        onRemove(entity, inst);
}

void BuffRuleDamageReduction::onAdd(Entity* entity, BuffInstance& inst)
{
    applyExtendDelta(entity, inst, ExtendAttributeType::AvoidHurt, param0(entity, inst));
}

void BuffRuleDamageReduction::onRemove(Entity* entity, BuffInstance& inst)
{
    clearExtend(entity, inst, ExtendAttributeType::AvoidHurt);
}

void BuffRuleDamageReduction::onBegin(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                                      int32_t skillId, float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    if (!inst.applied)
        onAdd(entity, inst);
}

void BuffRuleDamageReduction::onEnd(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                                    int32_t /*skillId*/, float /*param*/)
{
    if (inst.applied)
        onRemove(entity, inst);
}

void BuffRuleDamageSlot::onAdd(Entity* entity, BuffInstance& inst)
{
    // 预计算：每满段槽 * param[0]；简化为当前牌组技能数 * param（无满段信息时用 1）
    auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
    int32_t fullSlots = 0;
    if (deck)
    {
        for (const auto& e : deck->skills)
        {
            if (e.releaseMax > 0 && e.releaseCount <= 0)
                ++fullSlots;
        }
    }
    if (fullSlots <= 0)
        fullSlots = 1;
    inst.appliedValue = param0(entity, inst) * static_cast<float>(fullSlots);
    // 实际 ADD_HURT 在 onBegin 挂上
}

void BuffRuleDamageSlot::onRemove(Entity* entity, BuffInstance& inst)
{
    if (inst.applied)
    {
        BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddHurt, -inst.appliedValue);
        inst.applied = false;
    }
    inst.appliedValue = 0.0f;
}

void BuffRuleDamageSlot::onBegin(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                                 int32_t skillId, float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    if (inst.applied || inst.appliedValue == 0.0f)
        return;
    // 仅当施法技能属于牌组时触发
    if (skillId > 0 && !BuffRuleUtil::findDeckEntry(entity, skillId))
        return;
    BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddHurt, inst.appliedValue);
    inst.applied = true;
}

void BuffRuleDamageSlot::onEnd(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                               int32_t /*skillId*/, float /*param*/)
{
    if (!inst.applied)
        return;
    BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddHurt, -inst.appliedValue);
    inst.applied = false;
}

void BuffRuleCDSkill::onAdd(Entity* entity, BuffInstance& inst)
{
    const float scale = param0(entity, inst);
    auto* deck        = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!deck)
        return;
    const int32_t bindId = inst.sourceSkillId;
    for (auto& e : deck->skills)
    {
        if (bindId > 0 && e.skillAttackId != bindId)
            continue;
        e.coldTimeScale += scale;
    }
    inst.appliedValue = scale;
    inst.applied      = true;
}

void BuffRuleCDSkill::onRemove(Entity* entity, BuffInstance& inst)
{
    if (!inst.applied)
        return;
    auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!deck)
        return;
    const int32_t bindId = inst.sourceSkillId;
    for (auto& e : deck->skills)
    {
        if (bindId > 0 && e.skillAttackId != bindId)
            continue;
        e.coldTimeScale -= inst.appliedValue;
        if (e.coldTimeScale < 0.01f)
            e.coldTimeScale = 0.01f;
    }
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleModifyCDSkill::onAdd(Entity* entity, BuffInstance& inst)
{
    const float scale = param0(entity, inst);
    auto* deck        = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!deck)
        return;
    const int32_t bindId = inst.sourceSkillId;
    for (auto& e : deck->skills)
    {
        if (bindId > 0 && e.skillAttackId != bindId)
            continue;
        const int32_t delta = static_cast<int32_t>(static_cast<float>(e.coolDownMaxMs) * scale);
        e.coolDownMs        = (std::max)(0, e.coolDownMs + delta);
    }
}

void BuffRuleModifyCDSkill::onBegin(Entity* entity, BuffInstance& inst, BFEvent /*event*/, Entity* /*other*/,
                                    int32_t skillId, float /*param*/)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!BuffRuleUtil::passTriggerGates(entity, inst, cfg, skillId))
        return;
    onAdd(entity, inst);
}

void BuffRuleTPConsumeScale::onAdd(Entity* entity, BuffInstance& inst)
{
    const float scale = param0(entity, inst);
    const int32_t bindId = inst.sourceSkillId;
    if (bindId > 0)
    {
        if (auto* entry = BuffRuleUtil::findDeckEntry(entity, bindId))
            entry->mpConsumeScale += scale;
    }
    else if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        attr->mpConsumeScale += scale;
    }
    inst.appliedValue = scale;
    inst.applied      = true;
}

void BuffRuleTPConsumeScale::onRemove(Entity* entity, BuffInstance& inst)
{
    if (!inst.applied)
        return;
    const int32_t bindId = inst.sourceSkillId;
    if (bindId > 0)
    {
        if (auto* entry = BuffRuleUtil::findDeckEntry(entity, bindId))
            entry->mpConsumeScale -= inst.appliedValue;
    }
    else if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        attr->mpConsumeScale -= inst.appliedValue;
    }
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleEPConsumeScale::onAdd(Entity* entity, BuffInstance& inst)
{
    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        const float scale = param0(entity, inst);
        attr->epConsumeScale += scale;
        inst.appliedValue = scale;
        inst.applied      = true;
    }
}

void BuffRuleEPConsumeScale::onRemove(Entity* entity, BuffInstance& inst)
{
    if (!inst.applied)
        return;
    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        attr->epConsumeScale -= inst.appliedValue;
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleCrit::onAdd(Entity* entity, BuffInstance& inst)
{
    applyExtendDelta(entity, inst, ExtendAttributeType::AddCrit, param0(entity, inst));
}

void BuffRuleCrit::onRemove(Entity* entity, BuffInstance& inst)
{
    clearExtend(entity, inst, ExtendAttributeType::AddCrit);
}

void BuffRuleHPMAX::onAdd(Entity* entity, BuffInstance& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    const float rate = param0(entity, inst);
    const float baseMax =
        attr->currentAttribute.hpMax > 0 ? static_cast<float>(attr->currentAttribute.hpMax) : attr->currentAttribute.hp;
    const float add = baseMax * rate;
    BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddMaxHp, add);
    attr->currentAttribute.hpMax = static_cast<int32_t>(baseMax + add);
    attr->currentAttribute.hp += attr->currentAttribute.hp * rate;
    inst.appliedValue = add;
    inst.applied      = true;
}

void BuffRuleHPMAX::onRemove(Entity* entity, BuffInstance& inst)
{
    if (!inst.applied)
        return;
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (attr)
    {
        BuffRuleUtil::modifyExtend(entity, ExtendAttributeType::AddMaxHp, -inst.appliedValue);
        attr->currentAttribute.hpMax =
            (std::max)(1, attr->currentAttribute.hpMax - static_cast<int32_t>(inst.appliedValue));
        if (attr->currentAttribute.hp > static_cast<float>(attr->currentAttribute.hpMax))
            attr->currentAttribute.hp = static_cast<float>(attr->currentAttribute.hpMax);
    }
    inst.applied      = false;
    inst.appliedValue = 0.0f;
}

void BuffRuleHP::onAdd(Entity* entity, BuffInstance& inst)
{
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    const float v = param0(entity, inst);
    if (v > 1.0f)
        attr->currentAttribute.hp += v;
    else
        attr->currentAttribute.hp +=
            static_cast<float>(attr->currentAttribute.hpMax) * v;
    if (attr->currentAttribute.hp < 0.0f)
        attr->currentAttribute.hp = 0.0f;
}

bool BuffRuleHP::onTick(Entity* entity, BuffInstance& inst, int32_t dtMs)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg || cfg->interval <= 0)
        return true;
    inst.tickAccumMs += dtMs;
    if (inst.tickAccumMs >= cfg->interval)
    {
        inst.tickAccumMs = 0;
        onAdd(entity, inst);
    }
    return true;
}

NS_MG_END
