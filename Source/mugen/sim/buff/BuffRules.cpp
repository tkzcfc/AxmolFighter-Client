#include "mugen/sim/buff/BuffInternal.h"
#include "mugen/sim/base/Rng.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/MacroDefinition.h"
#include <algorithm>

namespace mugen::sim::buff::rules
{
namespace
{
using namespace conf::battle;
using detail::Exec;
using detail::buffDef;
using detail::destroySerial;
using detail::evalPredicate;
using detail::execOp;
using detail::findInstance;
using detail::onHook;
using detail::runOps;
using detail::triggerAbility;

int32_t constexpr kEnter = static_cast<int32_t>(BuffHook::Enter);
int32_t constexpr kTick   = static_cast<int32_t>(BuffHook::Tick);

// 区域过滤。空或首项 −1 表示全部通过。
bool areaOk(const std::vector<int32_t>& mask, int32_t fightType)
{
    if (mask.empty() || mask.front() == -1)
        return true;
    bool listed = false;
    for (size_t i = 1; i < mask.size(); ++i)
        if (mask[i] == fightType)
            listed = true;
    if (mask.front() == 0)
        return !listed;
    if (mask.front() == 1)
        return listed;
    return true;
}

// 准入条件。监视类条件在 Watch 上，这里只看激活条件。
bool conditionsOk(World& world, Entity entity, uint32_t serial, const BuffDef& def)
{
    Exec exec;
    exec.m_world  = &world;
    exec.m_self   = entity;
    exec.m_serial = serial;
    for (const auto& pred : def.m_activation.m_conditions)
        if (!evalPredicate(exec, pred))
            return false;
    return true;
}

// 去掉一层。最后一层或一次清全部时删除实例。
void dropLayer(World& world, Entity entity, uint32_t serial)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->m_id) : nullptr;
    if (!inst || !def)
        return;
    if (inst->m_stacks <= 1 || def->m_admission.m_removeAllStacks)
    {
        destroySerial(world, entity, serial);
        return;
    }
    --inst->m_stacks;
    Exec exec;
    exec.m_world  = &world;
    exec.m_self   = entity;
    exec.m_serial = serial;
    runOps(exec, def->m_onExit);
    inst = findInstance(world, entity, serial);
    if (!inst || !inst->m_latched)
        return;
    if (world.ecs().has<Stats>(entity))
    {
        auto& mods = world.ecs().get<Stats>(entity).m_mods;
        std::erase_if(mods, [serial](const StatMod& mod) { return mod.m_source == serial; });
    }
    for (const auto& op : def->m_persistent)
        if (op.m_kind == BuffOpKind::Stat || op.m_kind == BuffOpKind::HpLock)
            execOp(exec, op);
}

// 重复添加。优先级不够则保持旧实例。
void resetInstance(World& world, Entity entity, uint32_t serial, const BuffDef& incoming)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* oldDef = inst ? buffDef(inst->m_id) : nullptr;
    if (!inst || !oldDef)
        return;
    if (oldDef->m_admission.m_priority > incoming.m_admission.m_priority)
        return;
    const int32_t oldId = inst->m_id;
    const auto* initial = buffDef(inst->m_initialId);
    const bool sustain  = oldDef->m_admission.m_ruleBuffType == 1;
    if (oldId != incoming.m_id)
        onHook(world, entity, serial, static_cast<int32_t>(BuffHook::Exit));
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    if (incoming.m_admission.m_resetType == 0)
        return;
    const int32_t reset = incoming.m_admission.m_resetType;
    const int32_t cap   = initial ? initial->m_admission.m_maxStacks : oldDef->m_admission.m_maxStacks;
    if (reset >= 4 && inst->m_stacks < cap)
        ++inst->m_stacks;
    if (reset == 1 || reset == 2 || reset == 6 || reset == 7)
        inst->m_times = 0;
    if (reset == 1 || reset == 3 || reset == 5 || reset == 7)
        inst->m_interval = {};
    if (!sustain || incoming.m_lifecycle.m_times == -1 ||
        (inst->m_maxTimes != -1 && inst->m_maxTimes - inst->m_times < incoming.m_lifecycle.m_times))
    {
        inst->m_maxTimes    = incoming.m_lifecycle.m_times;
        inst->m_maxInterval = incoming.m_lifecycle.m_intervalSec;
    }
    inst->m_id = incoming.m_id;
    if (oldId != incoming.m_id || reset >= 4)
    {
        const bool dead = world.ecs().has<Health>(entity) && world.ecs().get<Health>(entity).m_dead;
        if ((incoming.m_admission.m_addType == 1 || !dead) && conditionsOk(world, entity, serial, incoming))
            onHook(world, entity, serial, kEnter);
    }
}

// 与本次事件匹配的实例。
bool accepts(World& world, Entity entity, BuffInstance& inst, const BuffDef& initial, const BuffDef& current,
             TriggerPoint point, const BuffContext& context)
{
    const bool begin = initial.m_activation.m_beginOn == point;
    const bool end   = initial.m_activation.m_endOn == point;
    if (!begin && !end)
        return false;
    const int32_t filter = current.m_activation.m_eventFilter;
    if (filter != 0 && inst.m_eventTarget != context.m_target)
    {
        if (filter == 8)
        {
            if (entity != context.m_target)
                return false;
        }
        else
        {
            int32_t campClass = 8;
            if (context.m_target.value != 0 && context.m_target != entity && world.ecs().has<Identity>(entity) &&
                world.ecs().has<Identity>(context.m_target))
            {
                campClass = world.ecs().get<Identity>(entity).m_camp ==
                                    world.ecs().get<Identity>(context.m_target).m_camp
                                ? 3
                                : 2;
            }
            if (campClass != filter)
                return false;
        }
    }
    if (current.m_admission.m_binding == 1 && inst.m_boundSkill != context.m_skillId)
        return false;
    return true;
}

// 把事件写进实例，保留施加者与绑定技能。
void writeEvent(BuffInstance& inst, const BuffContext& context)
{
    const Entity applicator = inst.m_applicator;
    const int32_t bound     = inst.m_boundSkill;
    inst.m_eventTarget      = context.m_target;
    inst.m_skillId          = context.m_skillId;
    inst.m_effectId         = context.m_effectId;
    inst.m_damage           = context.m_damage;
    inst.m_tpSpent          = context.m_tpSpent;
    inst.m_critical         = context.m_critical;
    inst.m_targetDead       = context.m_targetDead;
    inst.m_applicator       = applicator;
    inst.m_boundSkill       = bound;
}

// 监视器。
void runWatches(World& world, Entity entity, uint32_t serial)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->m_id) : nullptr;
    if (!inst || !def)
        return;
    Exec exec;
    exec.m_world  = &world;
    exec.m_self   = entity;
    exec.m_serial = serial;
    uint32_t bits = inst->m_watchBits;
    for (size_t i = 0; i < def->m_watch.size(); ++i)
    {
        const auto& op = def->m_watch[i];
        if (op.m_kind == BuffOpKind::Watch)
        {
            bool pass = true;
            for (const auto& pred : op.m_predicates)
                if (!evalPredicate(exec, pred))
                    pass = false;
            const uint32_t mask = 1u << static_cast<uint32_t>(i);
            const bool prev     = (bits & mask) != 0;
            if (op.m_watch == WatchMode::Latched && pass == prev)
                continue;
            if (pass)
                bits |= mask;
            else
                bits &= ~mask;
            for (const int32_t id : op.m_ids)
            {
                if (pass)
                    remove(world, entity, id);
                else
                {
                    BuffContext child;
                    child.m_applicator = inst->m_applicator;
                    child.m_boundSkill = inst->m_boundSkill;
                    admit(world, entity, id, child);
                }
            }
            inst = findInstance(world, entity, serial);
            if (!inst)
                return;
        }
        else if (op.m_kind == BuffOpKind::StateTimer && world.ecs().has<RoleState>(entity))
        {
            const auto state = world.ecs().get<RoleState>(entity).m_state;
            const bool match = (op.m_state == StateKind::GetUp && state == RoleStateId::GetUp) ||
                               (op.m_state == StateKind::HitRepel &&
                                (state == RoleStateId::Hit || state == RoleStateId::HitDown ||
                                 state == RoleStateId::HitSwitch)) ||
                               (op.m_state == StateKind::HitFly && state == RoleStateId::HitUp);
            Fixed* seconds = op.m_state == StateKind::GetUp     ? &inst->m_getUpSec
                             : op.m_state == StateKind::HitRepel ? &inst->m_repelSec
                                                                 : &inst->m_flySec;
            bool* done     = op.m_state == StateKind::GetUp     ? &inst->m_getUpDone
                             : op.m_state == StateKind::HitRepel ? &inst->m_repelDone
                                                                 : &inst->m_flyDone;
            if (!match)
            {
                *seconds = {};
                *done    = false;
                continue;
            }
            *seconds += world.ecs().singleton<WorldClock>().m_dtSec;
            if (!*done && *seconds >= op.m_f0)
            {
                *done = true;
                for (const int32_t id : op.m_ids)
                {
                    BuffContext child;
                    child.m_applicator = inst->m_applicator;
                    admit(world, entity, id, child);
                }
            }
        }
    }
    inst = findInstance(world, entity, serial);
    if (inst)
        inst->m_watchBits = bits;
}

// 周期推进。语义与旧的秒制生命周期一致。
void advanceOne(World& world, Entity entity, uint32_t serial, Fixed seconds)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->m_id) : nullptr;
    if (!inst || !def || inst->m_removing)
        return;
    const bool dead = world.ecs().has<Health>(entity) && world.ecs().get<Health>(entity).m_dead;
    const Entity applicator = inst->m_applicator;
    if (def->m_admission.m_addType == 2 &&
        (dead || (world.ecs().alive(applicator) && world.ecs().has<Health>(applicator) &&
                  world.ecs().get<Health>(applicator).m_dead)))
    {
        destroySerial(world, entity, serial);
        return;
    }
    if (dead && def->m_admission.m_addType != 1)
        return;
    if (!inst->m_active || inst->m_maxTimes < -1 || inst->m_maxInterval < Fixed{} ||
        (inst->m_maxTimes == 0 && inst->m_maxInterval == Fixed{}))
        return;
    if (inst->m_maxInterval > Fixed{})
    {
        if (inst->m_interval < inst->m_maxInterval)
        {
            inst->m_interval += seconds;
            // 初次 Enter 已把次数计满，本帧就掉一层（旧实现 finishEnter 在每次推进后都查）。
            if (inst->m_maxTimes > 0 && inst->m_times >= inst->m_maxTimes)
            {
                inst->m_times = 0;
                dropLayer(world, entity, serial);
            }
            return;
        }
        inst->m_interval -= inst->m_maxInterval;
    }
    if (inst->m_maxTimes == -1)
    {
        if (inst->m_innerCd > Fixed{})
        {
            // 超大内置冷却在定点里饱和到最大值。累加器已等于上限时再加 dt 会溢出；
            // 已达上限即视为本次到期，与先加后比且加法不溢出时的结果相同。
            if (inst->m_innerElapsed < inst->m_innerCd)
            {
                inst->m_innerElapsed += seconds;
                if (inst->m_innerElapsed < inst->m_innerCd)
                    return;
            }
            inst->m_innerElapsed = {};
        }
        if (conditionsOk(world, entity, serial, *def))
            onHook(world, entity, serial, kTick);
        return;
    }
    if (inst->m_maxTimes == 0)
    {
        dropLayer(world, entity, serial);
        return;
    }
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    ++inst->m_times;
    if (conditionsOk(world, entity, serial, *def))
        onHook(world, entity, serial, kEnter);
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    if (inst->m_maxTimes > 0 && inst->m_times >= inst->m_maxTimes)
    {
        inst->m_times = 0;
        dropLayer(world, entity, serial);
    }
}
}  // namespace

// 准入一个 Buff。
void admit(World& world, Entity entity, int32_t buffId, const BuffContext& context)
{
    if (!world.ecs().alive(entity) || world.ecs().has<Inactive>(entity) || world.ecs().has<Despawning>(entity))
        return;
    if (!world.ecs().has<BuffList>(entity))
        return;
    const auto* def = buffDef(buffId);
    if (!def || def->m_status == LowerStatus::Unsupported)
        return;
    auto& list = world.ecs().get<BuffList>(entity);
    for (const auto& immune : list.m_immunity)
    {
        if (immune.m_sel == static_cast<int32_t>(ImmuneSel::All))
            return;
        if (immune.m_sel == static_cast<int32_t>(ImmuneSel::Rules) &&
            (immune.m_key == -1 || immune.m_key == def->m_admission.m_ruleId))
            return;
        if (immune.m_sel == static_cast<int32_t>(ImmuneSel::BuffTypes) && immune.m_key == def->m_admission.m_buffType)
            return;
    }
    for (const auto& cd : list.m_cooldowns)
        if (cd.m_id == buffId && cd.m_left > Fixed{})
            return;
    if (!areaOk(def->m_admission.m_areaMask, world.setup().m_fightType))
        return;

    Fixed probability = def->m_admission.m_probability;
    if (def->m_admission.m_probabilityRepeat == 1 && def->m_admission.m_subType != -1)
        for (const auto& row : list.m_instances)
        {
            const auto* old = buffDef(row.m_id);
            if (!row.m_removing && old && old->m_admission.m_ruleId == def->m_admission.m_ruleId &&
                old->m_admission.m_subType == def->m_admission.m_subType)
            {
                probability *= Fixed::fromInt(row.m_stacks);
                break;
            }
        }
    const Fixed draw = rng::draw(world, rng::DrawKind::BuffProbability);
    // 概率到 100 一定通过。抽样闭区间含 100，严格小于会让必中行偶尔失败。
    if (probability < Fixed::fromInt(100) && !(draw < probability))
        return;

    uint32_t serial = 0;
    BuffInstance* existing = nullptr;
    if (def->m_admission.m_subType != -1)
        for (auto& row : list.m_instances)
        {
            const auto* old = buffDef(row.m_id);
            if (!row.m_removing && old && old->m_admission.m_ruleId == def->m_admission.m_ruleId &&
                old->m_admission.m_subType == def->m_admission.m_subType)
            {
                existing = &row;
                break;
            }
        }
    if (!existing)
    {
        BuffInstance created;
        created.m_serial       = list.m_nextSerial++;
        created.m_id           = buffId;
        created.m_initialId    = buffId;
        created.m_applicator   = context.m_applicator.value != 0 ? context.m_applicator : entity;
        created.m_boundSkill   = context.m_boundSkill;
        created.m_eventTarget  = context.m_target;
        created.m_skillId      = context.m_skillId;
        created.m_maxTimes     = def->m_lifecycle.m_times;
        created.m_maxInterval  = def->m_lifecycle.m_intervalSec;
        created.m_innerCd      = def->m_lifecycle.m_innerCdSec;
        created.m_innerElapsed = def->m_lifecycle.m_innerCdSec;
        serial                 = created.m_serial;
        list.m_instances.push_back(created);
        auto* added = findInstance(world, entity, serial);
        if (def->m_lifecycle.m_executeType == 1)
        {
            added->m_active = true;
            if (added->m_maxTimes > 0)
                ++added->m_times;
            const bool dead = world.ecs().has<Health>(entity) && world.ecs().get<Health>(entity).m_dead;
            if ((def->m_admission.m_addType == 1 || !dead) && conditionsOk(world, entity, serial, *def))
                onHook(world, entity, serial, kEnter);
        }
        else if (def->m_lifecycle.m_executeType == 2)
            added->m_active = true;
    }
    else
    {
        serial = existing->m_serial;
        resetInstance(world, entity, serial, *def);
    }

    const Fixed cd = world.setup().m_pvp ? def->m_admission.m_cdPvp : def->m_admission.m_cd;
    if (cd > Fixed{})
        world.ecs().get<BuffList>(entity).m_cooldowns.push_back({buffId, cd});
    BuffEvent event;
    event.m_entity = entity;
    event.m_buffId = buffId;
    event.m_serial = serial;
    event.m_phase  = BuffPhase::Added;
    world.ecs().singleton<EventQueue<BuffEvent>>().push(event);
    BuffContext added;
    added.m_target = entity;
    fire(world, entity, TriggerPoint::AddBuff, added);
}

// 移除指定 id 的全部实例。
void remove(World& world, Entity entity, int32_t buffId)
{
    if (!world.ecs().has<BuffList>(entity))
        return;
    std::vector<uint32_t> serials;
    for (const auto& inst : world.ecs().get<BuffList>(entity).m_instances)
        if (inst.m_id == buffId && !inst.m_removing)
            serials.push_back(inst.m_serial);
    for (const uint32_t serial : serials)
        destroySerial(world, entity, serial);
}

// 按规则移除。
void removeRule(World& world, Entity entity, int32_t ruleId, bool all)
{
    if (!world.ecs().has<BuffList>(entity))
        return;
    std::vector<uint32_t> serials;
    for (const auto& inst : world.ecs().get<BuffList>(entity).m_instances)
    {
        const auto* def = buffDef(inst.m_id);
        if (!inst.m_removing && def && def->m_admission.m_ruleId == ruleId)
            serials.push_back(inst.m_serial);
    }
    for (const uint32_t serial : serials)
    {
        if (all)
            destroySerial(world, entity, serial);
        else
            dropLayer(world, entity, serial);
    }
}

// 同步触发。
void fire(World& world, Entity entity, TriggerPoint point, const BuffContext& context)
{
    auto& chain = world.buffChain();
    if (chain.m_depth >= 8)
    {
        MG_LOG_E("Buff trigger chain exceeded depth 8");
        return;
    }
    ++chain.m_depth;
    if (point != TriggerPoint::BehaviorStateStart && point != TriggerPoint::BehaviorStateEnd &&
        world.ecs().has<BuffList>(entity))
    {
        std::vector<uint32_t> serials;
        for (const auto& inst : world.ecs().get<BuffList>(entity).m_instances)
            if (!inst.m_removing)
                serials.push_back(inst.m_serial);
        for (const uint32_t serial : serials)
        {
            const uint64_t key = (static_cast<uint64_t>(serial) << 32) | static_cast<uint32_t>(point);
            if (std::find(chain.m_seen.begin(), chain.m_seen.end(), key) != chain.m_seen.end())
                continue;
            auto* inst = findInstance(world, entity, serial);
            if (!inst)
                continue;
            const auto* current = buffDef(inst->m_id);
            const auto* initial = buffDef(inst->m_initialId == 0 ? inst->m_id : inst->m_initialId);
            if (!current || !initial || !accepts(world, entity, *inst, *initial, *current, point, context))
                continue;
            chain.m_seen.push_back(key);
            writeEvent(*inst, context);
            const bool ended = initial->m_activation.m_endOn == point;
            const bool dead  = world.ecs().has<Health>(entity) && world.ecs().get<Health>(entity).m_dead;
            const bool permitted = conditionsOk(world, entity, serial, *current);
            if (!ended && (!permitted || (current->m_admission.m_addType != 1 && dead)))
                continue;
            inst->m_active = true;
            onHook(world, entity, serial, ended ? static_cast<int32_t>(BuffHook::End) : static_cast<int32_t>(BuffHook::Begin));
            inst = findInstance(world, entity, serial);
            if (inst && current->m_activation.m_oneShot && (ended || initial->m_activation.m_endOn == TriggerPoint::None))
                destroySerial(world, entity, serial);
        }
    }
    if (world.ecs().has<AbilityList>(entity))
    {
        std::vector<uint32_t> serials;
        for (const auto& ability : world.ecs().get<AbilityList>(entity).m_instances)
            serials.push_back(ability.m_serial);
        for (const uint32_t serial : serials)
        {
            auto& list = world.ecs().get<AbilityList>(entity);
            AbilityInstance* ability = nullptr;
            for (auto& row : list.m_instances)
                if (row.m_serial == serial)
                    ability = &row;
            if (!ability)
                continue;
            const auto* def = conf::Config::instance().find<AbilityDef>(ability->m_id);
            if (!def)
                continue;
            if (std::find(def->m_events.begin(), def->m_events.end(), point) == def->m_events.end())
                continue;
            triggerAbility(world, entity, *ability, *def);
        }
    }
    if (--chain.m_depth == 0)
        chain.m_seen.clear();
}

// 召唤物死亡时撤销主人的链接，并执行死亡脉冲。
void notifySummonDeath(World& world, Entity owner)
{
    if (!world.ecs().has<BuffList>(owner))
        return;
    std::vector<uint32_t> linked;
    std::vector<uint32_t> pulses;
    for (const auto& inst : world.ecs().get<BuffList>(owner).m_instances)
    {
        const auto* def = buffDef(inst.m_id);
        if (!def || inst.m_removing)
            continue;
        for (const auto& op : def->m_persistent)
            if (op.m_kind == BuffOpKind::SummonLink && inst.m_latched)
                linked.push_back(inst.m_serial);
        bool pulse = false;
        if (inst.m_latched)
            for (const auto& op : def->m_persistent)
                if (op.m_kind == BuffOpKind::OnSummonDeath)
                    pulse = true;
        for (const auto& op : def->m_onEnter)
            if (op.m_kind == BuffOpKind::OnSummonDeath)
                pulse = true;
        if (pulse)
            pulses.push_back(inst.m_serial);
    }
    for (const uint32_t serial : pulses)
    {
        auto* inst = findInstance(world, owner, serial);
        const auto* def = inst ? buffDef(inst->m_id) : nullptr;
        if (!inst || !def)
            continue;
        auto apply = [&](const std::vector<BuffOp>& ops) {
            for (const auto& op : ops)
                if (op.m_kind == BuffOpKind::OnSummonDeath)
                    for (const int32_t id : op.m_ids)
                        admit(world, owner, id, {});
        };
        apply(def->m_onEnter);
        apply(def->m_persistent);
    }
    for (const uint32_t serial : linked)
        destroySerial(world, owner, serial);
}

// 每帧推进 Buff 与特殊能力。
void tick(World& world)
{
    // 与旧实现一致：dtMs/1000。m_dtSec 是另一个字面量，120 帧会提前跨过整秒门槛。
    const Fixed seconds = world.ecs().singleton<WorldClock>().m_dtMs / "1000"_fx;
    std::vector<Entity> holders;
    world.ecs().each<BuffList>([&](Entity entity, const BuffList&) { holders.push_back(entity); });
    for (const Entity entity : holders)
    {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            continue;
        std::vector<uint32_t> serials;
        for (const auto& inst : world.ecs().get<BuffList>(entity).m_instances)
            if (!inst.m_removing)
                serials.push_back(inst.m_serial);
        for (const uint32_t serial : serials)
        {
            advanceOne(world, entity, serial, seconds);
            runWatches(world, entity, serial);
        }
        if (!world.ecs().has<BuffList>(entity))
            continue;
        auto& cds = world.ecs().get<BuffList>(entity).m_cooldowns;
        for (auto& cd : cds)
            cd.m_left = std::max(Fixed{}, cd.m_left - seconds);
        std::erase_if(cds, [](const BuffCd& cd) { return cd.m_left <= Fixed{}; });
    }
    world.ecs().each<AbilityList>([&](Entity entity, AbilityList& list) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        std::vector<uint32_t> ready;
        for (auto& ability : list.m_instances)
        {
            if (ability.m_cdLeft > Fixed{})
                ability.m_cdLeft = std::max(Fixed{}, ability.m_cdLeft - seconds);
            const auto* def = conf::Config::instance().find<AbilityDef>(ability.m_id);
            if (def && def->m_intervalSec > Fixed{})
            {
                ability.m_elapsed += seconds;
                if (ability.m_elapsed >= def->m_intervalSec)
                {
                    ability.m_elapsed -= def->m_intervalSec;
                    ready.push_back(ability.m_serial);
                }
            }
            if (ability.m_endLeft >= Fixed{})
                ability.m_endLeft -= seconds;
        }
        for (const uint32_t serial : ready)
        {
            AbilityInstance* ability = nullptr;
            for (auto& row : list.m_instances)
                if (row.m_serial == serial)
                    ability = &row;
            if (!ability)
                continue;
            const auto* def = conf::Config::instance().find<AbilityDef>(ability->m_id);
            if (def)
                triggerAbility(world, entity, *ability, *def);
        }
        std::erase_if(list.m_instances, [](const AbilityInstance& ability) {
            return ability.m_endLeft <= Fixed{} && ability.m_endLeft != Fixed::fromInt(-1);
        });
    });
}
}  // namespace mugen::sim::buff::rules
