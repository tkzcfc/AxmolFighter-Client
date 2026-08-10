#include "mugen/buff/BuffApi.h"

#include "mugen/buff/BuffRuleFactory.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>

NS_MG_BEGIN

namespace BuffApi
{

namespace
{

std::string resolveClassName(const BuffConfig* cfg)
{
    if (!cfg)
        return {};
    if (!cfg->className.empty())
        return cfg->className;
    if (cfg->ruleId > 0)
    {
        if (const auto* rule = Config::getInstance()->getBuffRuleConfigById(cfg->ruleId))
            return rule->className;
    }
    return {};
}

}  // namespace

BuffRuleBase* resolveRule(const BuffInstance& inst)
{
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    std::string name = resolveClassName(cfg);
    if (name.empty() && cfg && cfg->interval > 0)
        name = "BuffPeriodicHurt";
    return BuffRuleFactory::instance().get(name);
}

bool addBuff(Entity* entity, int32_t buffId, int32_t sourceSkillId, int32_t level)
{
    if (!entity || buffId <= 0)
        return false;
    auto* buffComp = MG_GET_COMPONENT(entity, BuffComponent);
    if (!buffComp)
        return false;

    const auto* cfg = Config::getInstance()->getBuffConfigById(buffId);
    if (!cfg)
        return false;

    const int32_t ruleId  = cfg->ruleId;
    const int32_t subType = cfg->subType;
    const int32_t repeatMax = cfg->repeatMax > 0 ? cfg->repeatMax : 1;

    BuffInstance* existing = nullptr;
    if (subType != -1)
    {
        for (auto& b : buffComp->buffs)
        {
            if (b.ruleId == ruleId && b.subType == subType)
            {
                existing = &b;
                break;
            }
        }
        if (!existing && ruleId <= 0)
        {
            for (auto& b : buffComp->buffs)
            {
                if (b.buffId == buffId)
                {
                    existing = &b;
                    break;
                }
            }
        }
    }

    if (existing)
    {
        existing->remainingMs = (cfg->times > 0) ? cfg->times : existing->remainingMs;
        existing->repeatCount = (std::min)(repeatMax, existing->repeatCount + 1);
        existing->stacks      = existing->repeatCount;
        existing->tickAccumMs = 0;
        existing->level       = level;
        if (sourceSkillId > 0)
            existing->sourceSkillId = sourceSkillId;
        if (auto* rule = resolveRule(*existing))
            rule->onStack(entity, *existing);
        return true;
    }

    BuffInstance inst;
    inst.buffId        = buffId;
    inst.ruleId        = ruleId;
    inst.subType       = subType;
    inst.remainingMs   = (cfg->times > 0) ? cfg->times : 0;  // 0 = 永久（不按剩余时间移除）
    inst.repeatCount   = 1;
    inst.stacks        = 1;
    inst.sourceSkillId = sourceSkillId;
    inst.level         = level;
    inst.tickAccumMs   = 0;
    inst.innerCdMs     = 0;

    buffComp->buffs.push_back(inst);
    auto& stored = buffComp->buffs.back();
    BuffRuleUtil::attachSpine(entity, stored);
    if (auto* rule = resolveRule(stored))
        rule->onAdd(entity, stored);
    return true;
}

void removeBuff(Entity* entity, int32_t buffId)
{
    if (!entity || buffId <= 0)
        return;
    auto* buffComp = MG_GET_COMPONENT(entity, BuffComponent);
    if (!buffComp)
        return;

    const auto* cfg = Config::getInstance()->getBuffConfigById(buffId);
    const bool removeAll = cfg && cfg->removeRepeatAll != 0;

    for (auto it = buffComp->buffs.begin(); it != buffComp->buffs.end();)
    {
        if (it->buffId != buffId)
        {
            ++it;
            continue;
        }

        if (!removeAll && it->repeatCount > 1)
        {
            --it->repeatCount;
            it->stacks = it->repeatCount;
            if (auto* rule = resolveRule(*it))
                rule->onStack(entity, *it);
            ++it;
            continue;
        }

        if (auto* rule = resolveRule(*it))
            rule->onRemove(entity, *it);
        BuffRuleUtil::detachSpine(entity, *it);
        it = buffComp->buffs.erase(it);
    }
}

void trigger(Entity* entity, BFEvent event, Entity* other, int32_t skillId, float param)
{
    if (!entity)
        return;
    auto* buffComp = MG_GET_COMPONENT(entity, BuffComponent);
    if (!buffComp)
        return;

    const int32_t ev = static_cast<int32_t>(event);
    for (auto& inst : buffComp->buffs)
    {
        const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
        auto* rule      = resolveRule(inst);
        if (!rule)
            continue;

        if (cfg && cfg->began >= 0 && cfg->began == ev)
            rule->onBegin(entity, inst, event, other, skillId, param);
        else if (cfg && cfg->ended >= 0 && cfg->ended == ev)
            rule->onEnd(entity, inst, event, other, skillId, param);
        else
            rule->onEvent(entity, inst, event, other, skillId, param);
    }
}

}  // namespace BuffApi

NS_MG_END
