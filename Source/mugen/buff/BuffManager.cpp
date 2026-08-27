#include "mugen/buff/BuffManager.h"

#include "mugen/buff/BuffRuleBase.h"
#include "mugen/buff/BuffRuleFactory.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/Components.h"
#include "mugen/component/BuffComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{

std::string resolveClassName(const BuffConfig* cfg)
{
    if (!cfg)
        return {};
    if (cfg->ruleId > 0)
    {
        if (const auto* rule = Config::getInstance()->getBuffRuleConfigById(cfg->ruleId))
            return rule->className;
    }
    return {};
}

void unloadBuff(Entity* entity, Buff& buff)
{
    if (auto* rule = BuffManager::resolveRule(buff))
        rule->onRemove(entity, buff);
    BuffRuleUtil::detachSpine(entity, buff);
}

}  // namespace

// 没有则创建空 manager
BuffManager* BuffManager::of(Entity* entity)
{
    if (!entity)
        return nullptr;
    auto* comp = MG_GET_COMPONENT(entity, BuffComponent);
    return comp ? comp->ensureManager() : nullptr;
}

BuffRuleBase* BuffManager::resolveRule(const Buff& buff)
{
    const auto* cfg  = Config::getInstance()->getBuffConfigById(buff.buffId);
    std::string name = resolveClassName(cfg);
    if (name.empty() && cfg && cfg->interval > 0)
        name = "BuffPeriodicHurt";
    return BuffRuleFactory::instance().get(name);
}

Buff* BuffManager::findBuff(int32_t buffId) const
{
    if (buffId <= 0)
        return nullptr;
    for (const auto& b : buffs)
    {
        if (b && b->buffId == buffId)
            return b.get();
    }
    return nullptr;
}

void BuffManager::update(Entity* entity, int32_t dtMs)
{
    for (auto it = buffs.begin(); it != buffs.end();)
    {
        Buff* b = it->get();
        if (!b)
        {
            it = buffs.erase(it);
            continue;
        }
        b->tick(entity, dtMs);
        if (!b->destroyed)
        {
            ++it;
            continue;
        }
        unloadBuff(entity, *b);
        it = buffs.erase(it);
    }
}

bool BuffManager::addBuff(Entity* entity, int32_t buffId, int32_t sourceSkillId, int32_t level)
{
    if (!entity || buffId <= 0)
        return false;

    const auto* cfg = Config::getInstance()->getBuffConfigById(buffId);
    if (!cfg)
        return false;

    const int32_t ruleId    = cfg->ruleId;
    const int32_t subType   = cfg->subType;
    const int32_t repeatMax = cfg->repeatMax > 0 ? cfg->repeatMax : 1;

    Buff* existing = nullptr;
    if (subType != -1)
    {
        for (auto& b : buffs)
        {
            if (b && b->ruleId == ruleId && b->subType == subType)
            {
                existing = b.get();
                break;
            }
        }
        if (!existing && ruleId <= 0)
        {
            for (auto& b : buffs)
            {
                if (b && b->buffId == buffId)
                {
                    existing = b.get();
                    break;
                }
            }
        }
    }

    if (existing)
    {
        const int32_t dur     = BuffRuleUtil::durationMs(cfg);
        existing->remainingMs = dur > 0 ? dur : existing->remainingMs;
        existing->repeatCount = (std::min)(repeatMax, existing->repeatCount + 1);
        existing->stacks      = existing->repeatCount;
        existing->tickAccumMs = 0;
        existing->level       = level;
        existing->destroyed   = false;
        if (sourceSkillId > 0)
            existing->sourceSkillId = sourceSkillId;
        if (auto* rule = resolveRule(*existing))
            rule->onStack(entity, *existing);
        return true;
    }

    auto node           = std::make_unique<Buff>();
    node->buffId        = buffId;
    node->ruleId        = ruleId;
    node->subType       = subType;
    node->remainingMs   = BuffRuleUtil::durationMs(cfg);
    node->repeatCount   = 1;
    node->stacks        = 1;
    node->sourceSkillId = sourceSkillId;
    node->level         = level;
    node->tickAccumMs   = 0;
    node->innerCdMs     = 0;

    Buff* stored = node.get();
    buffs.push_back(std::move(node));
    BuffRuleUtil::attachSpine(entity, *stored);
    if (auto* rule = resolveRule(*stored))
        rule->onAdd(entity, *stored);
    return true;
}

void BuffManager::removeBuff(Entity* entity, int32_t buffId)
{
    if (!entity || buffId <= 0)
        return;

    const auto* cfg      = Config::getInstance()->getBuffConfigById(buffId);
    const bool removeAll = cfg && cfg->removeRepeatAll != 0;

    for (auto it = buffs.begin(); it != buffs.end();)
    {
        Buff* b = it->get();
        if (!b || b->buffId != buffId)
        {
            ++it;
            continue;
        }

        if (!removeAll && b->repeatCount > 1)
        {
            --b->repeatCount;
            b->stacks = b->repeatCount;
            if (auto* rule = resolveRule(*b))
                rule->onStack(entity, *b);
            ++it;
            continue;
        }

        unloadBuff(entity, *b);
        it = buffs.erase(it);
    }
}

void BuffManager::trigger(Entity* entity, BFEvent event, Entity* other, int32_t skillId, float param)
{
    if (!entity)
        return;

    const int32_t ev = static_cast<int32_t>(event);
    for (size_t i = 0; i < buffs.size(); ++i)
    {
        Buff* b = buffs[i].get();
        if (!b)
            continue;
        const auto* cfg = Config::getInstance()->getBuffConfigById(b->buffId);
        auto* rule      = resolveRule(*b);
        if (!rule)
            continue;

        if (cfg && cfg->began >= 0 && cfg->began == ev)
            rule->onBegin(entity, *b, event, other, skillId, param);
        else if (cfg && cfg->ended >= 0 && cfg->ended == ev)
            rule->onEnd(entity, *b, event, other, skillId, param);
        else
            rule->onEvent(entity, *b, event, other, skillId, param);
    }
}

void BuffManager::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeUint16(static_cast<uint16_t>(buffs.size()));
    for (const auto& b : buffs)
    {
        MG_ASSERT(b && "BuffManager: null Buff");
        b->serialize(byteBuffer);
    }
}

bool BuffManager::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    uint16_t count = 0;
    if (!byteBuffer.getUint16(count))
        return false;
    buffs.clear();
    buffs.reserve(count);
    for (uint16_t i = 0; i < count; ++i)
    {
        auto node = std::make_unique<Buff>();
        if (!node->deserialize(byteBuffer))
            return false;
        buffs.push_back(std::move(node));
    }
    return true;
}

NS_MG_END
