#include "BuffInstance.h"
#include "BuffPresentation.h"
#include "BuffCondition.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
// 移层时每次回调后重新定位实例，允许回调删除或追加池元素。
void removeRepeat(World& world, Entity entity, uint64_t serial, bool all, int32_t count, bool force)
{
    auto* instance = findInstance(world, entity, serial);
    if (!instance || count <= 0) return;
    const auto* initial = conf::Config::instance().find<conf::BuffConfig>(
        instance->m_initialId == 0 ? instance->m_id : instance->m_initialId);
    if (!initial) return;
    if (count >= instance->m_lifecycle.m_repeat) { force = false; all = true; }
    const bool removeAll = (all || initial->removeRepeatAll == 1) && !force;
    const int32_t calls = removeAll ? std::max(0, instance->m_lifecycle.m_repeat - 1) : count;
    for (int32_t index = 0; index < calls; ++index)
    {
        instance = findInstance(world, entity, serial);
        if (!instance) return;
        instance->m_lifecycle.m_repeat -= removeAll ? count : 1;
        invokeRule(world, entity, serial, RuleHook::Exit);
        instance = findInstance(world, entity, serial);
        if (!instance) return;
        if (!instance->m_triggered) instance->m_triggered = true;
    }
    instance = findInstance(world, entity, serial);
    if (!instance) return;
    instance->m_lifecycle.m_repeat = removeAll ? 0 : std::max(0, instance->m_lifecycle.m_repeat);
    if (instance->m_lifecycle.m_repeat == 0)
    {
        instance->m_lifecycle.m_interval = {};
        exitInstance(world, entity, serial);
    }
}

// 每次调用重新取得配置与实例，避免跨同步回调保存池元素引用。
void invokeRule(World& world, Entity entity, uint64_t serial, RuleHook hook, Fixed delta)
{
    const auto* instance = findInstance(world, entity, serial);
    if (!instance) return;
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    const auto* rule = config ? conf::Config::instance().find<conf::BuffRuleConfig>(config->ruleId) : nullptr;
    if (!rule) return;
    const auto id = conf::buffRuleIdFromName(rule->className);
    MG_ASSERT(id.has_value());
    if (!id) return;
    ruleDispatch(*id).m_hooks[static_cast<size_t>(hook)](world, entity, serial, delta);
}

// 重置零模式在旧实例退出钩子之后提前返回，不替换其配置标识。
void resetInstance(World& world, Entity entity, uint64_t serial, const conf::BuffConfig& config)
{
    auto* instance = findInstance(world, entity, serial);
    if (!instance) return;
    const auto oldId = instance->m_id;
    if (instance->m_initialId == 0) instance->m_initialId = oldId;
    const auto* initialConfig = conf::Config::instance().find<conf::BuffConfig>(instance->m_initialId);
    const auto* oldConfig = conf::Config::instance().find<conf::BuffConfig>(oldId);
    if (!oldConfig || oldConfig->priority > config.priority) return;
    const auto* oldRule = conf::Config::instance().find<conf::BuffRuleConfig>(oldConfig->ruleId);
    const bool sustain = oldRule && oldRule->buffType == 1;
    if (oldId != config.id) invokeRule(world, entity, serial, RuleHook::Exit);
    if (config.resetType == 0) return;
    auto& timers = world.ecs().singleton<LogicTimers>().entries;
    std::erase_if(timers, [entity, serial](const LogicTimer& timer) {
        const auto* exit = std::get_if<BuffExit>(&timer.payload);
        return timer.owner == entity && exit && exit->m_serial == serial;
    });
    instance = findInstance(world, entity, serial);
    if (!instance) return;
    if (config.resetType >= 4)
    {
        if (instance->m_lifecycle.m_repeat < (initialConfig ? initialConfig->repeatMax : oldConfig->repeatMax))
        {
            ++instance->m_lifecycle.m_repeat;
            if (instance->m_triggered && oldConfig->ended == -1)
                instance->m_triggered = false;
        }
        if (execute(instance->m_lifecycle, oldConfig->executeType) == LifecycleAction::Enter &&
            (oldConfig->addType == 1 || !Role(world, entity).isDead()) && checkConditions(world, entity, *instance))
            invokeRule(world, entity, serial, RuleHook::Enter);
    }
    instance = findInstance(world, entity, serial);
    if (!instance) return;
    auto& state = instance->m_lifecycle;
    resetCounters(state, config.resetType);
    if (!sustain || config.times == -1 ||
        (state.m_maxTimes != -1 && state.m_maxTimes - state.m_times < config.times))
    {
        state.m_maxTimes = config.times;
        state.m_maxInterval = config.interval;
    }
    state.m_destroy = false;
    state.m_exit = false;
    const bool restartPresentation = state.m_repeat == 0;
    if (restartPresentation) state.m_repeat = 1;
    instance->m_id = config.id;
    const auto* newRule = conf::Config::instance().find<conf::BuffRuleConfig>(config.ruleId);
    const bool shield = newRule && conf::buffRuleIdFromName(newRule->className) == conf::BuffRuleId::BuffShield;
    if (oldId != config.id || shield) invokeRule(world, entity, serial, RuleHook::Enter);
    if (restartPresentation || sustain || shield) beginPresentation(world, entity, serial);
}
}
