#include "BuffEvents.h"
#include "BuffCondition.h"
#include "BuffPresentation.h"
#include "BuffInstance.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/Hit.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
// 不把行为状态当作普通 began/ended 广播，避免其他规则错误响应。
void dispatchBehaviorState(World& world, Entity entity, BFEvent event, const char* actionName)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances)
    {
        const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance.m_id);
        const auto* rule = row ? conf::Config::instance().find<conf::BuffRuleConfig>(row->ruleId) : nullptr;
        if (rule && conf::buffRuleIdFromName(rule->className) == conf::BuffRuleId::BuffAddByState)
            serials.push_back(instance.m_serial);
    }
    for (const auto serial : serials)
    {
        auto* instance = findInstance(world, entity, serial);
        if (!instance) continue;
        instance->m_behaviorAction = actionName;
        instance->m_behaviorEvent = event;
        invokeRule(world, entity, serial, event == BFEvent::BehaviorStateStart ? RuleHook::Begin : RuleHook::End);
    }
}

// 固定本次广播的序号列表，新增实例不递归接收当前广播。
void dispatchEvent(World& world, Entity entity, BFEvent event, const BuffContext& context, int32_t targetCamp)
{
    if (event == BFEvent::BehaviorStateStart || event == BFEvent::BehaviorStateEnd)
    {
        dispatchBehaviorState(world, entity, event, "AttackRole");
        return;
    }
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances) serials.push_back(instance.m_serial);
    for (const auto serial : serials)
    {
        auto* instance = findInstance(world, entity, serial);
        if (!instance) continue;
        const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
        const auto* initial = conf::Config::instance().find<conf::BuffConfig>(
            instance->m_initialId == 0 ? instance->m_id : instance->m_initialId);
        if (!config || !initial || !acceptsEvent(*config, *initial, *instance, entity, event, context, targetCamp)) continue;
        const auto bound = instance->m_context.m_boundSkill;
        const auto front = instance->m_context.m_frontSkill;
        const auto applicator = instance->m_context.m_applicator;
        instance->m_context = context;
        instance->m_context.m_boundSkill = bound;
        instance->m_context.m_frontSkill = front;
        instance->m_context.m_applicator = applicator;
        const bool permitted = checkConditions(world, entity, *instance);
        if (static_cast<int32_t>(event) == 9)
        {
            if (instance->m_lifecycle.m_active && !permitted) exitInstance(world, entity, serial);
            instance = findInstance(world, entity, serial);
            if (!instance || instance->m_lifecycle.m_active || !permitted) continue;
        }
        else if (static_cast<int32_t>(event) == 10 && (!permitted || !instance->m_lifecycle.m_active)) continue;
        if (config->bindSpecialAbilityId != -1 && config->bindSpecialAbilityId != 0 && world.hitHooks().m_buffAbility)
        {
            const auto abilityContext = instance->m_context;
            world.hitHooks().m_buffAbility(world, entity, config->bindSpecialAbilityId, event, abilityContext);
            instance = findInstance(world, entity, serial);
            if (!instance) continue;
        }
        instance->m_lifecycle.m_active = true;
        const bool ended = static_cast<int32_t>(event) == initial->ended;
        if (!ended && (!permitted || (config->addType != 1 && hit::isDead(world, entity)))) continue;
        invokeRule(world, entity, serial, ended ? RuleHook::End : RuleHook::Begin);
        if (config->eventParam.size() > 1 && config->eventParam[1] == 1 && (ended || config->ended == -1))
            exitInstance(world, entity, serial);
    }
}

// 已记录的相同目标跳过阵营过滤；绑定技能必须与当前事件技能一致。
bool acceptsEvent(const conf::BuffConfig& current, const conf::BuffConfig& initial,
                  const BuffInstance& instance, Entity owner, BFEvent event,
                  const BuffContext& context, int32_t targetCamp)
{
    const auto eventId = static_cast<int32_t>(event);
    if (eventId != initial.began && eventId != initial.ended) return false;
    const int32_t targetFilter = current.eventParam.empty() ? 0 : current.eventParam.front();
    if (instance.m_context.m_target != context.m_target && targetFilter != 0)
    {
        if (targetFilter == 8)
        {
            if (owner != context.m_target) return false;
        }
        else if (targetCamp != targetFilter) return false;
    }
    if (current.binding == 1 && instance.m_context.m_boundSkill != context.m_currentSkill)
        return false;
    return true;
}
}
