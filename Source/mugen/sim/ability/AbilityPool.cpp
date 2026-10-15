#include "AbilityHolder.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffComponents.h"
#include "mugen/sim/hit/HitResolver.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/sim/skill/SkillComponents.h"

namespace mugen::sim::ability
{
namespace
{
// 以稳定序号重新查找可能被同步回调改变的实例。
AbilityInstance* find(AbilityPool& pool, uint64_t serial)
{
    for (auto& instance : pool.m_instances)
        if (instance.m_serial == serial) return &instance;
    return nullptr;
}
// 判断配置事件列表是否接受当前广播。
bool accepts(const conf::SpecialAbilityConfig& config, Event event)
{
    return std::find(config.event.begin(), config.event.end(), static_cast<int32_t>(event)) != config.event.end();
}
// 将能力效果实体标记为能力来源，避免其命中递归触发能力。
void addEffects(World& world, Entity entity, const conf::SpecialAbilityConfig& config, const AbilityContext& context)
{
    for (size_t index = 0; index < config.effectId.size(); ++index)
    {
        const int32_t id = config.effectId[index];
        if (id <= 0) continue;
        const int32_t targetType = index < config.effectIdType.size() ? config.effectIdType[index] : 1;
        const Entity target = targetType == 1 ? entity : context.m_target;
        if (target.value == 0) continue;
        const Entity effectEntity = effect::EffectActor::create(world, entity, id);
        if (effectEntity.value == 0) continue;
        auto& state = world.ecs().get<EffectState>(effectEntity);
        state.m_capabilityTrigger = true;
        effect::EffectActor(world, effectEntity).setCustomPosition(target);
        effect::EffectActor(world, effectEntity).enter();
    }
}
// 按配置目标类型添加关联 Buff。
void addBuffs(World& world, Entity entity, const conf::SpecialAbilityConfig& config, const AbilityContext& context)
{
    for (size_t index = 0; index < config.bindBuffId.size(); ++index)
    {
        const int32_t id = config.bindBuffId[index];
        if (id <= 0) continue;
        const int32_t targetType = index < config.bindBuffType.size() ? config.bindBuffType[index] : 1;
        const Entity target = targetType == 1 ? entity : context.m_target;
        if (target.value != 0) hit::HitResolver(world, target).addEntityBuff(id);
    }
}
}

AbilityHolder::AbilityHolder(World& world, Entity entity) : m_world(world), m_entity(entity) {}

// 条件数组采用逻辑与；空数组与 None 条件都通过。
bool AbilityHolder::checkConditions(const conf::SpecialAbilityConfig& config, const AbilityContext& context) const
{
    if (config.condition == -1) return true;
    if (config.condition == 1) return context.m_critical;
    if (config.condition == 2)
    {
        const auto* external = m_world.ecs().tryGet<BuffExternalConditions>(m_entity);
        return external && external->m_comboEvaluate >= config.conditionValue;
    }
    if (config.condition == 3)
    {
        const auto* skills = m_world.ecs().tryGet<SkillPool>(m_entity);
        return skills && skills->m_crazy;
    }
    return false;
}

// 复用普通子类型，负一子类型始终新建独立实例。
std::optional<uint64_t> AbilityHolder::add(int32_t id, std::optional<int32_t> boundSkill)
{
    const auto* config = conf::Config::instance().find<conf::SpecialAbilityConfig>(id);
    if (!config) return std::nullopt;
    if (!m_world.ecs().has<AbilityPool>(m_entity)) m_world.ecs().add(m_entity, AbilityPool{});
    auto& pool = m_world.ecs().get<AbilityPool>(m_entity);
    if (config->subType != -1)
        for (auto& instance : pool.m_instances)
            if (instance.m_id == id)
            {
                if (boundSkill) instance.m_boundSkill = boundSkill;
                return instance.m_serial;
            }
    AbilityInstance instance;
    instance.m_serial = pool.m_nextSerial++;
    instance.m_id = id;
    instance.m_boundSkill = boundSkill;
    pool.m_instances.push_back(instance);
    const AbilityContext context;
    _activate(instance.m_serial, Event::InitAbility, context);
    return instance.m_serial;
}

// 先固定序号列表，新增能力不接收当前事件。
void AbilityHolder::trigger(Event event, const AbilityContext& context)
{
    const auto* pool = m_world.ecs().tryGet<AbilityPool>(m_entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances) serials.push_back(instance.m_serial);
    for (const auto serial : serials) _activate(serial, event, context);
}

// 配置指定的能力先确保存在，再只向该实例派发事件。
void AbilityHolder::triggerId(int32_t id, Event event, const AbilityContext& context)
{
    const auto serial = add(id, context.m_skillId == -1 ? std::optional<int32_t>{} : std::optional<int32_t>{context.m_skillId});
    if (serial) _activate(*serial, event, context);
}

// 计时以秒推进，结束实例在本次更新末尾稳定删除。
void AbilityHolder::update()
{
    auto* pool = m_world.ecs().tryGet<AbilityPool>(m_entity);
    if (!pool) return;
    const Fixed seconds = m_world.ecs().singleton<WorldClock>().dtMs / "1000"_fx;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances) serials.push_back(instance.m_serial);
    for (const auto serial : serials)
    {
        auto* instance = find(*pool, serial);
        if (!instance) continue;
        const auto* config = conf::Config::instance().find<conf::SpecialAbilityConfig>(instance->m_id);
        if (!config) continue;
        instance->m_cooldown = std::max(Fixed{}, instance->m_cooldown - seconds);
        if (config->interval > 0)
        {
            instance->m_interval += seconds;
            if (instance->m_interval >= Fixed::fromInt(config->interval))
            {
                instance->m_interval -= Fixed::fromInt(config->interval);
                _activate(serial, Event::Time, instance->m_context);
            }
        }
        instance = find(*pool, serial);
        if (instance && config->endedType == 1) instance->m_ended -= seconds;
    }
    std::erase_if(pool->m_instances, [](const AbilityInstance& instance) {
        return instance.m_ended < Fixed{};
    });
}

// 角色卸载时直接清空能力值状态。
void AbilityHolder::clear()
{
    if (auto* pool = m_world.ecs().tryGet<AbilityPool>(m_entity)) pool->m_instances.clear();
}

// 在满足全部门槛后执行一次能力动作。
void AbilityHolder::_activate(uint64_t serial, Event event, const AbilityContext& context)
{
    auto& pool = m_world.ecs().get<AbilityPool>(m_entity);
    auto* instance = find(pool, serial);
    if (!instance) return;
    const auto* config = conf::Config::instance().find<conf::SpecialAbilityConfig>(instance->m_id);
    if (!config || !accepts(*config, event) || instance->m_cooldown > Fixed{}) return;
    if (config->bindSkillType != 0 && instance->m_boundSkill != context.m_skillId) return;
    if (!checkConditions(*config, context)) return;
    if (config->probability < "100"_fx && m_world.ecs().singleton<RngState>().random.randomF("0"_fx, "100"_fx) > config->probability)
        return;
    instance->m_context = context;
    instance->m_active = true;
    instance->m_cooldown = m_world.setup().m_pvp ? config->SpecialAbilityCDPvp : config->SpecialAbilityCD;
    if (instance->m_cooldown < Fixed{}) instance->m_cooldown = {};
    if (config->endedType == 1) instance->m_ended = config->endedValue;
    addEffects(m_world, m_entity, *config, context);
    addBuffs(m_world, m_entity, *config, context);
}
}  // namespace mugen::sim::ability
