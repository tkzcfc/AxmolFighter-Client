#include "BuffPool.h"
#include "BuffInstance.h"
#include "BuffCondition.h"
#include "BuffEvents.h"
#include "BuffPresentation.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/World.h"
#include "mugen/sim/trace/TraceSink.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
// 先保存目标句柄，再执行同步施加，避免回调扩容使遍历引用失效。
void addBuffForApplicator(World& world, Entity entity, int32_t id, const BuffContext& context,
                          int32_t fightType, bool notify)
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(id);
    if (!config) return;
    const auto identity = world.ecs().get<RoleIdentity>(entity);
    std::vector<Entity> targets;
    if (config->target == 1 || (config->target == 8 && context.m_target == entity)) targets.push_back(entity);
    else world.ecs().each<RoleIdentity>([&](Entity candidate, const RoleIdentity& other) {
        const bool enemy = (identity.hostileCamp & other.camp) != 0;
        const bool selected = (config->target == 2 && enemy && candidate == context.m_target) ||
            (config->target == 3 && !enemy && candidate == context.m_target) ||
            (config->target == 4 && !enemy) || (config->target == 5 && enemy) ||
            (config->target == 6 && other.roleType == 4) ||
            (config->target == 7 && (other.roleType == 2 || other.roleType == 3)) || config->target == 9;
        if (selected) targets.push_back(candidate);
    });
    for (const auto target : targets)
    {
        auto source = context;
        source.m_target = {};
        if (config->target >= 4 && config->target != 8) source.m_boundSkill.reset();
        if (config->target == 2 || config->target == 3 || config->target == 8) source.m_applicator = entity;
        addBuff(world, target, id, source, fightType, notify);
    }
}

// 技能池和角色必需组件已建立后才允许初始效果同步触发回调。
void loadBuffs(World& world, Entity entity, const std::vector<LoadEntry>& additional)
{
    if (!world.ecs().has<BuffPool>(entity)) world.ecs().add(entity, BuffPool{});
    const auto identity = world.ecs().get<RoleIdentity>(entity);
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(identity.roleId);
    if (!row) return;
    for (const auto id : row->buffIds)
        if (id != -1) addEntityBuff(world, entity, id);
    for (const auto& entry : additional)
        if (entry.m_id > 0) addEntityBuff(world, entity, entry.m_id, entry.m_skill, entry.m_origin);
    if (identity.roleType == static_cast<int32_t>(EntityRoleType::kHero))
        if (const auto* room = conf::Config::instance().find<conf::RoomConfig>(world.setup().roomId))
            for (const auto id : room->actorBuffIds)
                if (id != -1) addEntityBuff(world, entity, id);
}

// 概率只从世界唯一随机源抽取，未通过准入检查时不消耗随机数。
std::optional<uint64_t> addBuff(World& world, Entity entity, int32_t id,
                               const BuffContext& context, int32_t fightType, bool notify)
{
    const auto* group = world.ecs().tryGet<EntityGroup>(entity);
    if (!group || group->stopped || group->destroyRequested) return std::nullopt;
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(id);
    if (!config || !buffCheck(world, entity, *config, fightType)) return std::nullopt;
    if (!conf::Config::instance().find<conf::BuffRuleConfig>(config->ruleId)) return std::nullopt;
    if (!world.ecs().has<BuffPool>(entity)) world.ecs().add(entity, BuffPool{});
    auto& pool = world.ecs().get<BuffPool>(entity);
    const auto draw = world.ecs().singleton<RngState>().random.randomF("0"_fx, "100"_fx);
    if (!probabilityPass(pool, *config, draw)) return std::nullopt;
    auto serial = findByType(pool, config->ruleId, config->subType);
    if (!serial || config->subType == -1)
    {
        BuffInstance instance;
        instance.m_serial = pool.m_nextSerial++;
        instance.m_id = instance.m_initialId = id;
        instance.m_context = context;
        instance.m_source = context.m_origin;
        instance.m_lifecycle = makeLifecycle(*config);
        serial = instance.m_serial;
        pool.m_instances.push_back(instance);
        beginPresentation(world, entity, *serial);
        auto* added = findInstance(world, entity, *serial);
        const auto action = execute(added->m_lifecycle, config->executeType);
        if (action == LifecycleAction::Enter && (config->addType == 1 || !Role(world, entity).isDead()) &&
            checkConditions(world, entity, *added)) invokeRule(world, entity, *serial, RuleHook::Enter);
    }
    else resetInstance(world, entity, *serial, *config);
    const auto cd = world.setup().m_pvp ? config->cdPvp : config->cd;
    if (cd != Fixed{}) world.ecs().get<BuffPool>(entity).m_cooldowns.push_back({id, cd});
    if (notify)
    {
        BuffContext event;
        event.m_target = entity;
        dispatchEvent(world, entity, BFEvent::AddBuff, event, 8);
    }
    trace::noteBuffAdd(world, entity, id);
    return serial;
}

// 对全部匹配标识依次移除，剩余单层直接进入退出阶段。
void removeBuff(World& world, Entity entity, int32_t id, bool all, bool notify)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances)
        if (instance.m_id == id) serials.push_back(instance.m_serial);
    for (const auto serial : serials)
    {
        const auto* instance = findInstance(world, entity, serial);
        if (!instance) continue;
        if (instance->m_lifecycle.m_repeat == 1) exitInstance(world, entity, serial);
        else removeRepeat(world, entity, serial, all);
        if (notify)
        {
            BuffContext context;
            context.m_target = entity;
            dispatchEvent(world, entity, BFEvent::RemoveBuff, context, 8);
        }
    }
}

// 周期回调和移层保持同步，完成后稳定移除已销毁实例。
void update(World& world, Entity entity)
{
    auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    const auto seconds = world.ecs().singleton<WorldClock>().dtMs / "1000"_fx;
    std::vector<uint64_t> serials;
    for (const auto& instance : pool->m_instances) serials.push_back(instance.m_serial);
    for (const auto serial : serials)
    {
        auto* instance = findInstance(world, entity, serial);
        if (!instance) continue;
        const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
        if (!config) continue;
        const bool dead = Role(world, entity).isDead();
        const auto applicator = instance->m_context.m_applicator;
        if (config->addType == 2 && (dead ||
            (world.ecs().has<Vitals>(applicator) && Role(world, applicator).isDead())))
        {
            destroyBuff(world, entity, instance->m_id);
            continue;
        }
        if (dead && config->addType != 1) continue;
        if (instance->m_lifecycle.m_maxInterval < Fixed{} || instance->m_lifecycle.m_maxTimes < -1)
            continue;
        const auto action = advance(instance->m_lifecycle, seconds);
        if (action == LifecycleAction::Enter && checkConditions(world, entity, *instance))
            invokeRule(world, entity, serial, RuleHook::Enter);
        else if (action == LifecycleAction::Update)
            invokeRule(world, entity, serial, RuleHook::Update, seconds);
        instance = findInstance(world, entity, serial);
        if (!instance) continue;
        if (action == LifecycleAction::RemoveRepeat ||
            (instance->m_lifecycle.m_active && !instance->m_lifecycle.m_exit && finishEnter(instance->m_lifecycle)))
            removeRepeat(world, entity, serial);
    }
    pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    for (const auto& instance : pool->m_instances)
        if (instance.m_lifecycle.m_destroy)
            trace::noteBuffRemove(world, entity, instance.m_id);
    std::erase_if(pool->m_instances, [](const BuffInstance& instance) { return instance.m_lifecycle.m_destroy; });
    updateCooldowns(*pool, seconds);
}

namespace
{
// 强制删除不等待表现，回调后重新取得组件以规避扩容失效。
void destroySerial(World& world, Entity entity, uint64_t serial, bool byRule)
{
    const auto* instance = findInstance(world, entity, serial);
    if (!instance) return;
    const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    if (!byRule && row && row->destroyType == 1 && instance->m_lifecycle.m_repeat > 1)
        removeRepeat(world, entity, serial, true);
    invokeRule(world, entity, serial, RuleHook::Exit);
    cancelAppearance(world, entity, serial);
    if (row && row->spineId != -1) world.emit(entity, BuffSpine{serial, row->spineId, 0, row->id});
    auto& timers = world.ecs().singleton<LogicTimers>().entries;
    std::erase_if(timers, [entity, serial](const LogicTimer& timer) {
        const auto* exit = std::get_if<BuffExit>(&timer.payload);
        return timer.owner == entity && exit && exit->m_serial == serial;
    });
    auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    const auto* removed = findInstance(world, entity, serial);
    const int32_t removedId = removed ? removed->m_id : 0;
    const auto before = pool->m_instances.size();
    std::erase_if(pool->m_instances, [serial](const BuffInstance& value) { return value.m_serial == serial; });
    if (removed && pool->m_instances.size() < before)
        trace::noteBuffRemove(world, entity, removedId);
}
}

// 先保存逆序实例标识，退出回调改变数组时不会误删其他实例。
void destroyBuff(World& world, Entity entity, std::optional<int32_t> id)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (auto it = pool->m_instances.rbegin(); it != pool->m_instances.rend(); ++it)
        if (!id || it->m_id == *id) serials.push_back(it->m_serial);
    for (const auto serial : serials) destroySerial(world, entity, serial, false);
}

// 规则销毁保持独立的退出一次语义，不执行配置的逐层销毁选项。
void destroyBuffByRule(World& world, Entity entity, int32_t ruleId)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return;
    std::vector<uint64_t> serials;
    for (auto it = pool->m_instances.rbegin(); it != pool->m_instances.rend(); ++it)
    {
        const auto* row = conf::Config::instance().find<conf::BuffConfig>(it->m_id);
        if (row && row->ruleId == ruleId) serials.push_back(it->m_serial);
    }
    for (const auto serial : serials) destroySerial(world, entity, serial, true);
}

// 同类匹配不排除退出标记，物理删除由池更新负责。
std::optional<uint64_t> findByType(const BuffPool& pool, int32_t ruleId, int32_t subType)
{
    for (const auto& instance : pool.m_instances)
    {
        const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance.m_id);
        if (row && row->ruleId == ruleId && row->subType == subType)
            return instance.m_serial;
    }
    return std::nullopt;
}

// 重复概率乘现有层数；没有同类实例时仍使用基础概率。
bool probabilityPass(const BuffPool& pool, const conf::BuffConfig& config, Fixed draw)
{
    Fixed probability = config.probability;
    if (config.probabilityRepeat == 1)
    {
        const auto serial = findByType(pool, config.ruleId, config.subType);
        if (serial)
            for (const auto& instance : pool.m_instances)
                if (instance.m_serial == *serial)
                {
                    probability *= Fixed::fromInt(instance.m_lifecycle.m_repeat);
                    break;
                }
    }
    return draw < probability;
}

// 保留池中的实例可被查询，销毁实例由更新阶段统一移除。
bool hasBuff(World& world, Entity entity, int32_t id)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return false;
    return std::any_of(pool->m_instances.begin(), pool->m_instances.end(),
                       [id](const BuffInstance& instance) { return instance.m_id == id; });
}

// 免疫集合按首次插入顺序维护，负一由检查入口解释为全部免疫。
void setImmune(BuffPool& pool, int32_t ruleId, bool immune)
{
    const auto found = std::find(pool.m_immunity.begin(), pool.m_immunity.end(), ruleId);
    if (immune && found == pool.m_immunity.end()) pool.m_immunity.push_back(ruleId);
    else if (!immune && found != pool.m_immunity.end()) pool.m_immunity.erase(found);
}

// 冷却先检查再递减，刚减到零的条目仍阻止本帧再次添加。
void updateCooldowns(BuffPool& pool, Fixed dtSeconds)
{
    for (size_t index = 0; index < pool.m_cooldowns.size();)
    {
        if (pool.m_cooldowns[index].m_seconds <= Fixed{})
            pool.m_cooldowns.erase(pool.m_cooldowns.begin() + index);
        else
        {
            pool.m_cooldowns[index].m_seconds -= dtSeconds;
            ++index;
        }
    }
}

// 配置链可能悬空，减益规则查询使用可空查找。
bool buffCheck(World& world, Entity entity, const conf::BuffConfig& config, int32_t fightType)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (pool)
    {
        for (const int32_t immune : pool->m_immunity)
            if (immune == -1 || immune == config.ruleId) return false;
        for (const auto& cooldown : pool->m_cooldowns)
            if (cooldown.m_id == config.id) return false;
        if (config.buffType == 3 || config.buffType == 4)
        {
            for (const auto& instance : pool->m_instances)
            {
                const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance.m_id);
                const auto* rule = row ? conf::Config::instance().find<conf::BuffRuleConfig>(row->ruleId) : nullptr;
                if (rule && conf::buffRuleIdFromName(rule->className) == conf::BuffRuleId::BuffImmuneDebuff)
                    return false;
            }
        }
    }
    if (config.areaSetting.empty() || config.areaSetting.front() == -1) return true;
    const bool listed = std::find(config.areaSetting.begin() + 1, config.areaSetting.end(), fightType)
        != config.areaSetting.end();
    if (config.areaSetting.front() == 0) return !listed;
    if (config.areaSetting.front() == 1) return listed;
    return true;
}
}
