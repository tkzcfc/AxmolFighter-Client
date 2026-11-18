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
    exec.world  = &world;
    exec.self   = entity;
    exec.serial = serial;
    for (const auto& pred : def.activation.conditions)
        if (!evalPredicate(exec, pred))
            return false;
    return true;
}

// 去掉一层。最后一层或一次清全部时删除实例。
void dropLayer(World& world, Entity entity, uint32_t serial)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->id) : nullptr;
    if (!inst || !def)
        return;
    if (inst->stacks <= 1 || def->admission.removeAllStacks)
    {
        destroySerial(world, entity, serial);
        return;
    }
    --inst->stacks;
    Exec exec;
    exec.world  = &world;
    exec.self   = entity;
    exec.serial = serial;
    runOps(exec, def->onExit);
    inst = findInstance(world, entity, serial);
    if (!inst || !inst->latched)
        return;
    if (world.ecs().has<StatsComponent>(entity))
    {
        auto& mods = world.ecs().get<StatsComponent>(entity).mods;
        std::erase_if(mods, [serial](const StatMod& mod) { return mod.source == serial; });
    }
    for (const auto& op : def->persistent)
        if (op.kind == BuffOpKind::Stat || op.kind == BuffOpKind::HpLock)
            execOp(exec, op);
}

// 重复添加。优先级不够则保持旧实例。
void resetInstance(World& world, Entity entity, uint32_t serial, const BuffDef& incoming)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* oldDef = inst ? buffDef(inst->id) : nullptr;
    if (!inst || !oldDef)
        return;
    if (oldDef->admission.priority > incoming.admission.priority)
        return;
    const int32_t oldId = inst->id;
    const auto* initial = buffDef(inst->initialId);
    const bool sustain  = oldDef->admission.ruleBuffType == 1;
    if (oldId != incoming.id)
        onHook(world, entity, serial, static_cast<int32_t>(BuffHook::Exit));
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    if (incoming.admission.resetType == 0)
        return;
    const int32_t reset = incoming.admission.resetType;
    const int32_t cap   = initial ? initial->admission.maxStacks : oldDef->admission.maxStacks;
    if (reset >= 4 && inst->stacks < cap)
        ++inst->stacks;
    if (reset == 1 || reset == 2 || reset == 6 || reset == 7)
        inst->times = 0;
    if (reset == 1 || reset == 3 || reset == 5 || reset == 7)
        inst->interval = {};
    if (!sustain || incoming.lifecycle.times == -1 ||
        (inst->maxTimes != -1 && inst->maxTimes - inst->times < incoming.lifecycle.times))
    {
        inst->maxTimes    = incoming.lifecycle.times;
        inst->maxInterval = incoming.lifecycle.intervalSec;
    }
    inst->id = incoming.id;
    if (oldId != incoming.id || reset >= 4)
    {
        const bool dead = world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead;
        if ((incoming.admission.addType == 1 || !dead) && conditionsOk(world, entity, serial, incoming))
            onHook(world, entity, serial, kEnter);
    }
}

// 与本次事件匹配的实例。
bool accepts(World& world, Entity entity, BuffInstance& inst, const BuffDef& initial, const BuffDef& current,
             TriggerPoint point, const BuffContext& context)
{
    const bool begin = initial.activation.beginOn == point;
    const bool end   = initial.activation.endOn == point;
    if (!begin && !end)
        return false;
    const int32_t filter = current.activation.eventFilter;
    if (filter != 0 && inst.eventTarget != context.target)
    {
        if (filter == 8)
        {
            if (entity != context.target)
                return false;
        }
        else
        {
            int32_t campClass = 8;
            if (context.target.value != 0 && context.target != entity && world.ecs().has<IdentityComponent>(entity) &&
                world.ecs().has<IdentityComponent>(context.target))
            {
                campClass = world.ecs().get<IdentityComponent>(entity).camp ==
                                    world.ecs().get<IdentityComponent>(context.target).camp
                                ? 3
                                : 2;
            }
            if (campClass != filter)
                return false;
        }
    }
    if (current.admission.binding == 1 && inst.boundSkill != context.skillId)
        return false;
    return true;
}

// 把事件写进实例，保留施加者与绑定技能。
void writeEvent(BuffInstance& inst, const BuffContext& context)
{
    const Entity applicator = inst.applicator;
    const int32_t bound     = inst.boundSkill;
    inst.eventTarget      = context.target;
    inst.skillId          = context.skillId;
    inst.effectId         = context.effectId;
    inst.damage           = context.damage;
    inst.tpSpent          = context.tpSpent;
    inst.critical         = context.critical;
    inst.targetDead       = context.targetDead;
    inst.applicator       = applicator;
    inst.boundSkill       = bound;
}

// 监视器。
void runWatches(World& world, Entity entity, uint32_t serial)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->id) : nullptr;
    if (!inst || !def)
        return;
    Exec exec;
    exec.world  = &world;
    exec.self   = entity;
    exec.serial = serial;
    uint32_t bits = inst->watchBits;
    for (size_t i = 0; i < def->watch.size(); ++i)
    {
        const auto& op = def->watch[i];
        if (op.kind == BuffOpKind::Watch)
        {
            bool pass = true;
            for (const auto& pred : op.predicates)
                if (!evalPredicate(exec, pred))
                    pass = false;
            const uint32_t mask = 1u << static_cast<uint32_t>(i);
            const bool prev     = (bits & mask) != 0;
            if (op.watch == WatchMode::Latched && pass == prev)
                continue;
            if (pass)
                bits |= mask;
            else
                bits &= ~mask;
            for (const int32_t id : op.ids)
            {
                if (pass)
                    remove(world, entity, id);
                else
                {
                    BuffContext child;
                    child.applicator = inst->applicator;
                    child.boundSkill = inst->boundSkill;
                    admit(world, entity, id, child);
                }
            }
            inst = findInstance(world, entity, serial);
            if (!inst)
                return;
        }
        else if (op.kind == BuffOpKind::StateTimer && world.ecs().has<RoleStateComponent>(entity))
        {
            const auto state = world.ecs().get<RoleStateComponent>(entity).state;
            const bool match = (op.state == StateKind::GetUp && state == RoleStateId::GetUp) ||
                               (op.state == StateKind::HitRepel &&
                                (state == RoleStateId::Hit || state == RoleStateId::HitDown ||
                                 state == RoleStateId::HitSwitch)) ||
                               (op.state == StateKind::HitFly && state == RoleStateId::HitUp);
            Fixed* seconds = op.state == StateKind::GetUp     ? &inst->getUpSec
                             : op.state == StateKind::HitRepel ? &inst->repelSec
                                                                 : &inst->flySec;
            bool* done     = op.state == StateKind::GetUp     ? &inst->getUpDone
                             : op.state == StateKind::HitRepel ? &inst->repelDone
                                                                 : &inst->flyDone;
            if (!match)
            {
                *seconds = {};
                *done    = false;
                continue;
            }
            *seconds += world.ecs().singleton<WorldClock>().dtSec;
            if (!*done && *seconds >= op.f0)
            {
                *done = true;
                for (const int32_t id : op.ids)
                {
                    BuffContext child;
                    child.applicator = inst->applicator;
                    admit(world, entity, id, child);
                }
            }
        }
    }
    inst = findInstance(world, entity, serial);
    if (inst)
        inst->watchBits = bits;
}

// 周期推进。语义与旧的秒制生命周期一致。
void advanceOne(World& world, Entity entity, uint32_t serial, Fixed seconds)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->id) : nullptr;
    if (!inst || !def || inst->removing)
        return;
    const bool dead = world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead;
    const Entity applicator = inst->applicator;
    if (def->admission.addType == 2 &&
        (dead || (world.ecs().alive(applicator) && world.ecs().has<HealthComponent>(applicator) &&
                  world.ecs().get<HealthComponent>(applicator).dead)))
    {
        destroySerial(world, entity, serial);
        return;
    }
    if (dead && def->admission.addType != 1)
        return;
    if (!inst->active || inst->maxTimes < -1 || inst->maxInterval < Fixed{} ||
        (inst->maxTimes == 0 && inst->maxInterval == Fixed{}))
        return;
    if (inst->maxInterval > Fixed{})
    {
        if (inst->interval < inst->maxInterval)
        {
            inst->interval += seconds;
            // 初次 Enter 已把次数计满，本帧就掉一层（旧实现 finishEnter 在每次推进后都查）。
            if (inst->maxTimes > 0 && inst->times >= inst->maxTimes)
            {
                inst->times = 0;
                dropLayer(world, entity, serial);
            }
            return;
        }
        inst->interval -= inst->maxInterval;
    }
    if (inst->maxTimes == -1)
    {
        if (inst->innerCd > Fixed{})
        {
            // 超大内置冷却在定点里饱和到最大值。累加器已等于上限时再加 dt 会溢出；
            // 已达上限即视为本次到期，与先加后比且加法不溢出时的结果相同。
            if (inst->innerElapsed < inst->innerCd)
            {
                inst->innerElapsed += seconds;
                if (inst->innerElapsed < inst->innerCd)
                    return;
            }
            inst->innerElapsed = {};
        }
        if (conditionsOk(world, entity, serial, *def))
            onHook(world, entity, serial, kTick);
        return;
    }
    if (inst->maxTimes == 0)
    {
        dropLayer(world, entity, serial);
        return;
    }
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    ++inst->times;
    if (conditionsOk(world, entity, serial, *def))
        onHook(world, entity, serial, kEnter);
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    if (inst->maxTimes > 0 && inst->times >= inst->maxTimes)
    {
        inst->times = 0;
        dropLayer(world, entity, serial);
    }
}
}  // namespace

// 准入一个 Buff。
void admit(World& world, Entity entity, int32_t buffId, const BuffContext& context)
{
    if (!world.ecs().alive(entity) || world.ecs().has<InactiveComponent>(entity) || world.ecs().has<DespawningComponent>(entity))
        return;
    if (!world.ecs().has<BuffListComponent>(entity))
        return;
    const auto* def = buffDef(buffId);
    if (!def || def->status == LowerStatus::Unsupported)
        return;
    auto& list = world.ecs().get<BuffListComponent>(entity);
    for (const auto& immune : list.immunity)
    {
        if (immune.sel == static_cast<int32_t>(ImmuneSel::All))
            return;
        if (immune.sel == static_cast<int32_t>(ImmuneSel::Rules) &&
            (immune.key == -1 || immune.key == def->admission.ruleId))
            return;
        if (immune.sel == static_cast<int32_t>(ImmuneSel::BuffTypes) && immune.key == def->admission.buffType)
            return;
    }
    for (const auto& cd : list.cooldowns)
        if (cd.id == buffId && cd.left > Fixed{})
            return;
    if (!areaOk(def->admission.areaMask, world.setup().fightType))
        return;

    Fixed probability = def->admission.probability;
    if (def->admission.probabilityRepeat == 1 && def->admission.subType != -1)
        for (const auto& row : list.instances)
        {
            const auto* old = buffDef(row.id);
            if (!row.removing && old && old->admission.ruleId == def->admission.ruleId &&
                old->admission.subType == def->admission.subType)
            {
                probability *= Fixed::fromInt(row.stacks);
                break;
            }
        }
    const Fixed draw = rng::draw(world, rng::DrawKind::BuffProbability);
    // 概率到 100 一定通过。抽样闭区间含 100，严格小于会让必中行偶尔失败。
    if (probability < Fixed::fromInt(100) && !(draw < probability))
        return;

    uint32_t serial = 0;
    BuffInstance* existing = nullptr;
    if (def->admission.subType != -1)
        for (auto& row : list.instances)
        {
            const auto* old = buffDef(row.id);
            if (!row.removing && old && old->admission.ruleId == def->admission.ruleId &&
                old->admission.subType == def->admission.subType)
            {
                existing = &row;
                break;
            }
        }
    if (!existing)
    {
        BuffInstance created;
        created.serial       = list.nextSerial++;
        created.id           = buffId;
        created.initialId    = buffId;
        created.applicator   = context.applicator.value != 0 ? context.applicator : entity;
        created.boundSkill   = context.boundSkill;
        created.eventTarget  = context.target;
        created.skillId      = context.skillId;
        created.maxTimes     = def->lifecycle.times;
        created.maxInterval  = def->lifecycle.intervalSec;
        created.innerCd      = def->lifecycle.innerCdSec;
        created.innerElapsed = def->lifecycle.innerCdSec;
        serial                 = created.serial;
        list.instances.push_back(created);
        auto* added = findInstance(world, entity, serial);
        if (def->lifecycle.executeType == 1)
        {
            added->active = true;
            if (added->maxTimes > 0)
                ++added->times;
            const bool dead = world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead;
            if ((def->admission.addType == 1 || !dead) && conditionsOk(world, entity, serial, *def))
                onHook(world, entity, serial, kEnter);
        }
        else if (def->lifecycle.executeType == 2)
            added->active = true;
    }
    else
    {
        serial = existing->serial;
        resetInstance(world, entity, serial, *def);
    }

    const Fixed cd = world.setup().pvp ? def->admission.cdPvp : def->admission.cd;
    if (cd > Fixed{})
        world.ecs().get<BuffListComponent>(entity).cooldowns.push_back({buffId, cd});
    BuffEvent event;
    event.entity = entity;
    event.buffId = buffId;
    event.serial = serial;
    event.phase  = BuffPhase::Added;
    world.ecs().singleton<EventQueue<BuffEvent>>().push(event);
    BuffContext added;
    added.target = entity;
    fire(world, entity, TriggerPoint::AddBuff, added);
}

// 移除指定 id 的全部实例。
void remove(World& world, Entity entity, int32_t buffId)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return;
    std::vector<uint32_t> serials;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
        if (inst.id == buffId && !inst.removing)
            serials.push_back(inst.serial);
    for (const uint32_t serial : serials)
        destroySerial(world, entity, serial);
}

// 按规则移除。
void removeRule(World& world, Entity entity, int32_t ruleId, bool all)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return;
    std::vector<uint32_t> serials;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
    {
        const auto* def = buffDef(inst.id);
        if (!inst.removing && def && def->admission.ruleId == ruleId)
            serials.push_back(inst.serial);
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
    if (chain.depth >= 8)
    {
        MG_LOG_E("Buff trigger chain exceeded depth 8");
        return;
    }
    ++chain.depth;
    if (point != TriggerPoint::BehaviorStateStart && point != TriggerPoint::BehaviorStateEnd &&
        world.ecs().has<BuffListComponent>(entity))
    {
        std::vector<uint32_t> serials;
        for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
            if (!inst.removing)
                serials.push_back(inst.serial);
        for (const uint32_t serial : serials)
        {
            const uint64_t key = (static_cast<uint64_t>(serial) << 32) | static_cast<uint32_t>(point);
            if (std::find(chain.seen.begin(), chain.seen.end(), key) != chain.seen.end())
                continue;
            auto* inst = findInstance(world, entity, serial);
            if (!inst)
                continue;
            const auto* current = buffDef(inst->id);
            const auto* initial = buffDef(inst->initialId == 0 ? inst->id : inst->initialId);
            if (!current || !initial || !accepts(world, entity, *inst, *initial, *current, point, context))
                continue;
            chain.seen.push_back(key);
            writeEvent(*inst, context);
            const bool ended = initial->activation.endOn == point;
            const bool dead  = world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead;
            const bool permitted = conditionsOk(world, entity, serial, *current);
            if (!ended && (!permitted || (current->admission.addType != 1 && dead)))
                continue;
            inst->active = true;
            onHook(world, entity, serial, ended ? static_cast<int32_t>(BuffHook::End) : static_cast<int32_t>(BuffHook::Begin));
            inst = findInstance(world, entity, serial);
            if (inst && current->activation.oneShot && (ended || initial->activation.endOn == TriggerPoint::None))
                destroySerial(world, entity, serial);
        }
    }
    if (world.ecs().has<AbilityListComponent>(entity))
    {
        std::vector<uint32_t> serials;
        for (const auto& ability : world.ecs().get<AbilityListComponent>(entity).instances)
            serials.push_back(ability.serial);
        for (const uint32_t serial : serials)
        {
            auto& list = world.ecs().get<AbilityListComponent>(entity);
            AbilityInstance* ability = nullptr;
            for (auto& row : list.instances)
                if (row.serial == serial)
                    ability = &row;
            if (!ability)
                continue;
            const auto* def = conf::Config::instance().find<AbilityDef>(ability->id);
            if (!def)
                continue;
            if (std::find(def->events.begin(), def->events.end(), point) == def->events.end())
                continue;
            triggerAbility(world, entity, *ability, *def);
        }
    }
    if (--chain.depth == 0)
        chain.seen.clear();
}

// 召唤物死亡时撤销主人的链接，并执行死亡脉冲。
void notifySummonDeath(World& world, Entity owner)
{
    if (!world.ecs().has<BuffListComponent>(owner))
        return;
    std::vector<uint32_t> linked;
    std::vector<uint32_t> pulses;
    for (const auto& inst : world.ecs().get<BuffListComponent>(owner).instances)
    {
        const auto* def = buffDef(inst.id);
        if (!def || inst.removing)
            continue;
        for (const auto& op : def->persistent)
            if (op.kind == BuffOpKind::SummonLink && inst.latched)
                linked.push_back(inst.serial);
        bool pulse = false;
        if (inst.latched)
            for (const auto& op : def->persistent)
                if (op.kind == BuffOpKind::OnSummonDeath)
                    pulse = true;
        for (const auto& op : def->onEnter)
            if (op.kind == BuffOpKind::OnSummonDeath)
                pulse = true;
        if (pulse)
            pulses.push_back(inst.serial);
    }
    for (const uint32_t serial : pulses)
    {
        auto* inst = findInstance(world, owner, serial);
        const auto* def = inst ? buffDef(inst->id) : nullptr;
        if (!inst || !def)
            continue;
        auto apply = [&](const std::vector<BuffOp>& ops) {
            for (const auto& op : ops)
                if (op.kind == BuffOpKind::OnSummonDeath)
                    for (const int32_t id : op.ids)
                        admit(world, owner, id, {});
        };
        apply(def->onEnter);
        apply(def->persistent);
    }
    for (const uint32_t serial : linked)
        destroySerial(world, owner, serial);
}

// 每帧推进 Buff 与特殊能力。
void tick(World& world)
{
    // 与旧实现一致：dtMs/1000。dtSec 是另一个字面量，120 帧会提前跨过整秒门槛。
    const Fixed seconds = world.ecs().singleton<WorldClock>().dtMs / "1000"_fx;
    std::vector<Entity> holders;
    world.ecs().each<BuffListComponent>([&](Entity entity, const BuffListComponent&) { holders.push_back(entity); });
    for (const Entity entity : holders)
    {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            continue;
        std::vector<uint32_t> serials;
        for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
            if (!inst.removing)
                serials.push_back(inst.serial);
        for (const uint32_t serial : serials)
        {
            advanceOne(world, entity, serial, seconds);
            runWatches(world, entity, serial);
        }
        if (!world.ecs().has<BuffListComponent>(entity))
            continue;
        auto& cds = world.ecs().get<BuffListComponent>(entity).cooldowns;
        for (auto& cd : cds)
            cd.left = std::max(Fixed{}, cd.left - seconds);
        std::erase_if(cds, [](const BuffCd& cd) { return cd.left <= Fixed{}; });
    }
    world.ecs().each<AbilityListComponent>([&](Entity entity, AbilityListComponent& list) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        std::vector<uint32_t> ready;
        for (auto& ability : list.instances)
        {
            if (ability.cdLeft > Fixed{})
                ability.cdLeft = std::max(Fixed{}, ability.cdLeft - seconds);
            const auto* def = conf::Config::instance().find<AbilityDef>(ability.id);
            if (def && def->intervalSec > Fixed{})
            {
                ability.elapsed += seconds;
                if (ability.elapsed >= def->intervalSec)
                {
                    ability.elapsed -= def->intervalSec;
                    ready.push_back(ability.serial);
                }
            }
            if (ability.endLeft >= Fixed{})
                ability.endLeft -= seconds;
        }
        for (const uint32_t serial : ready)
        {
            AbilityInstance* ability = nullptr;
            for (auto& row : list.instances)
                if (row.serial == serial)
                    ability = &row;
            if (!ability)
                continue;
            const auto* def = conf::Config::instance().find<AbilityDef>(ability->id);
            if (def)
                triggerAbility(world, entity, *ability, *def);
        }
        std::erase_if(list.instances, [](const AbilityInstance& ability) {
            return ability.endLeft <= Fixed{} && ability.endLeft != Fixed::fromInt(-1);
        });
    });
}
}  // namespace mugen::sim::buff::rules
