#include "AbilityPool.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffComponents.h"
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
        const Entity effectEntity = effect::create(world, entity, id);
        if (effectEntity.value == 0) continue;
        auto& state = world.ecs().get<EffectState>(effectEntity);
        state.m_capabilityTrigger = true;
        effect::setCustomPosition(world, effectEntity, target);
        effect::enter(world, effectEntity);
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
        if (target.value != 0) buff::addEntityBuff(world, target, id);
    }
}
// 在满足全部门槛后执行一次能力动作。
void activate(World& world, Entity entity, uint64_t serial, Event event, const AbilityContext& context)
{
    auto& pool = world.ecs().get<AbilityPool>(entity);
    auto* instance = find(pool, serial);
    if (!instance) return;
    const auto* config = conf::Config::instance().find<conf::SpecialAbilityConfig>(instance->m_id);
    if (!config || !accepts(*config, event) || instance->m_cooldown > Fixed{}) return;
    if (config->bindSkillType != 0 && instance->m_boundSkill != context.m_skillId) return;
    if (!checkConditions(world, entity, *config, context)) return;
    if (config->probability < "100"_fx && world.ecs().singleton<RngState>().random.randomF("0"_fx, "100"_fx) > config->probability)
        return;
    instance->m_context = context;
    instance->m_active = true;
    instance->m_cooldown = world.setup().m_pvp ? config->SpecialAbilityCDPvp : config->SpecialAbilityCD;
    if (instance->m_cooldown < Fixed{}) instance->m_cooldown = {};
    if (config->endedType == 1) instance->m_ended = config->endedValue;
    addEffects(world, entity, *config, context);
    addBuffs(world, entity, *config, context);
}
}

// 条件数组采用逻辑与；空数组与 None 条件都通过。
bool checkConditions(World& world, Entity entity, const conf::SpecialAbilityConfig& config, const AbilityContext& context)
{
    if (config.condition == -1) return true;
    if (config.condition == 1) return context.m_critical;
    if (config.condition == 2)
    {
        const auto* external = world.ecs().tryGet<BuffExternalConditions>(entity);
        return external && external->m_comboEvaluate >= config.conditionValue;
    }
    if (config.condition == 3)
    {
        const auto* skills = world.ecs().tryGet<SkillPool>(entity);
        return skills && skills->m_crazy;
    }
    return false;
}

// 复用普通子类型，负一子类型始终新建独立实例。
std::optional<uint64_t> add(World& world, Entity entity, int32_t id, std::optional<int32_t> boundSkill)
{
    const auto* config = conf::Config::instance().find<conf::SpecialAbilityConfig>(id);
    if (!config) return std::nullopt;
    if (!world.ecs().has<AbilityPool>(entity)) world.ecs().add(entity, AbilityPool{});
    auto& pool = world.ecs().get<AbilityPool>(entity);
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
    activate(world, entity, instance.m_serial, Event::InitAbility, context);
    return instance.m_serial;
}

// 先固定序号列表，新增能力不接收当前事件。
void trigger(World& world, Entity entity, Event event, const AbilityContext& context)
{
    const auto* pool = world.ecs().tryGet<AbilityPool>(entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances) serials.push_back(instance.m_serial);
    for (const auto serial : serials) activate(world, entity, serial, event, context);
}

// 配置指定的能力先确保存在，再只向该实例派发事件。
void triggerId(World& world, Entity entity, int32_t id, Event event, const AbilityContext& context)
{
    const auto serial = add(world, entity, id, context.m_skillId == -1 ? std::optional<int32_t>{} : std::optional<int32_t>{context.m_skillId});
    if (serial) activate(world, entity, *serial, event, context);
}

// 计时以秒推进，结束实例在本次更新末尾稳定删除。
void update(World& world, Entity entity)
{
    auto* pool = world.ecs().tryGet<AbilityPool>(entity);
    if (!pool) return;
    const Fixed seconds = world.ecs().singleton<WorldClock>().dtMs / "1000"_fx;
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
                activate(world, entity, serial, Event::Time, instance->m_context);
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
void clear(World& world, Entity entity)
{
    if (auto* pool = world.ecs().tryGet<AbilityPool>(entity)) pool->m_instances.clear();
}
}  // namespace mugen::sim::ability
