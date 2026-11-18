#include "mugen/sim/buff/BuffInternal.h"
#include "mugen/sim/base/Rng.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/motion/MotionComponents.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/MacroDefinition.h"
#include <algorithm>

namespace mugen::sim::buff::detail
{
namespace
{
using namespace conf::battle;

// 根实体：召唤链的最外层，否则自身。
Entity rootOf(World& world, Entity entity)
{
    if (!world.ecs().alive(entity) || !world.ecs().has<IdentityComponent>(entity))
        return entity;
    const auto& identity = world.ecs().get<IdentityComponent>(entity);
    if (world.ecs().alive(identity.rootOwner) && world.ecs().has<StatsComponent>(identity.rootOwner))
        return identity.rootOwner;
    if (world.ecs().alive(identity.owner) && world.ecs().has<StatsComponent>(identity.owner))
        return identity.owner;
    return entity;
}

// 登记或替换一条属性修正。
void putStat(World& world, Entity entity, int32_t stat, Fixed value, uint32_t source, bool replace)
{
    if (!world.ecs().has<StatsComponent>(entity))
        return;
    auto& mods = world.ecs().get<StatsComponent>(entity).mods;
    if (replace)
    {
        for (auto& mod : mods)
            if (mod.source == source && mod.stat == stat)
            {
                mod.value = value;
                return;
            }
    }
    mods.push_back({stat, value, source});
}

// 技能修正是否作用到这一条技能。
bool skillInScope(const SkillBookComponent& book, const SkillRuntime& skill, const SkillMod& mod, int32_t bound)
{
    const auto name = skill.slot >= 0 && static_cast<size_t>(skill.slot) < book.slots.size()
                          ? book.slots[static_cast<size_t>(skill.slot)]
                          : std::string{};
    switch (static_cast<SkillScope>(mod.scope))
    {
    case SkillScope::Bound:
        return bound > 0 && skill.configId == bound;
    case SkillScope::Slots:
    case SkillScope::ArtifactSlots:
        return mod.slot == skill.slot;
    case SkillScope::BaseSlotsCrazyPlus:
        return name == "A" || name == "B" || name == "C" || name == "D" || name == "E" || name == "F" || name == "Y";
    default:
        return true;
    }
}

// 表现开关。
void visualCue(World& world, Entity entity, VisualKind kind, int32_t on)
{
    CueKind cue = CueKind::Tip;
    if (kind == VisualKind::WhiteFrame)
        cue = CueKind::WhiteFrame;
    else if (kind == VisualKind::StealthView)
        cue = CueKind::StealthView;
    else if (kind == VisualKind::SuperArmorOutline)
        cue = CueKind::SuperArmorOutline;
    else if (kind == VisualKind::InkScreen)
        cue = CueKind::InkScreen;
    else
        return;
    CueEvent event;
    event.entity = entity;
    event.kind   = cue;
    event.param  = on;
    world.ecs().singleton<EventQueue<CueEvent>>().push(event);
}

// 阵营关系。
bool hostile(const IdentityComponent& self, const IdentityComponent& other)
{
    return (static_cast<uint32_t>(self.hostileMask) & static_cast<uint32_t>(other.camp)) != 0;
}

bool sameCamp(const IdentityComponent& self, const IdentityComponent& other)
{
    return self.camp == other.camp;
}

// 按选择器收集角色。
std::vector<Entity> selectTargets(Exec& exec, int32_t selector)
{
    auto& world = *exec.world;
    auto* inst  = findInstance(world, exec.self, exec.serial);
    const Entity eventTarget = inst ? inst->eventTarget : Entity{};
    const Entity self        = exec.self;
    std::vector<Entity> out;
    if (!world.ecs().has<IdentityComponent>(self))
        return out;
    const auto& selfId = world.ecs().get<IdentityComponent>(self);
    auto accept        = [&](Entity entity, const IdentityComponent& identity) {
        if (world.ecs().has<InactiveComponent>(entity) || world.ecs().has<DespawningComponent>(entity))
            return;
        if (identity.kind != SpawnKind::Role)
            return;
        switch (selector)
        {
        case 1:
            if (entity == self)
                out.push_back(entity);
            break;
        case 2:
            if (entity == eventTarget)
                out.push_back(entity);
            break;
        case 3:
            if (world.ecs().alive(eventTarget) && world.ecs().has<IdentityComponent>(eventTarget) &&
                sameCamp(world.ecs().get<IdentityComponent>(eventTarget), identity))
                out.push_back(entity);
            break;
        case 4:
            if (sameCamp(selfId, identity))
                out.push_back(entity);
            break;
        case 5:
            if (hostile(selfId, identity))
                out.push_back(entity);
            break;
        case 6:
            if (identity.roleType == static_cast<int32_t>(mugen::EntityRoleType::kBoss))
                out.push_back(entity);
            break;
        case 7:
            if (identity.roleType == static_cast<int32_t>(mugen::EntityRoleType::kMonster) ||
                identity.roleType == static_cast<int32_t>(mugen::EntityRoleType::kElite))
                out.push_back(entity);
            break;
        case 8:
            if (entity == self && eventTarget == self)
                out.push_back(entity);
            break;
        case 9:
            out.push_back(entity);
            break;
        case 10:
            if (entity != self && hostile(selfId, identity))
                out.push_back(entity);
            break;
        default:
            break;
        }
    };
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) { accept(entity, identity); });
    return out;
}

// 在矩形内。
bool inArea(World& world, Entity origin, Entity other, Fixed rx, Fixed rz)
{
    if (!world.ecs().has<TransformComponent>(origin) || !world.ecs().has<TransformComponent>(other))
        return false;
    const auto& a = world.ecs().get<TransformComponent>(origin).position;
    const auto& b = world.ecs().get<TransformComponent>(other).position;
    const Fixed dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const Fixed dz = a.z > b.z ? a.z - b.z : b.z - a.z;
    return dx <= rx && dz <= rz;
}

// 按权重选一个下标。DrawPolicy 下固定取 0。
int32_t pickWeighted(World& world, const std::vector<Fixed>& weights)
{
    if (weights.empty())
        return 0;
    Fixed total{};
    for (const Fixed weight : weights)
        if (weight > Fixed{})
            total += weight;
    if (total <= Fixed{})
        return 0;
    const Fixed draw = rng::draw(world, rng::DrawKind::Choice) / Fixed::fromInt(100) * total;
    Fixed walked{};
    for (size_t i = 0; i < weights.size(); ++i)
    {
        if (weights[i] <= Fixed{})
            continue;
        walked += weights[i];
        if (draw < walked)
            return static_cast<int32_t>(i);
    }
    return static_cast<int32_t>(weights.size() - 1);
}

// 冷却目标技能。
void eachCdSkill(World& world, Entity entity, const BuffOp& op, int32_t bound, const auto& fn)
{
    if (!world.ecs().has<SkillBookComponent>(entity))
        return;
    auto& book = world.ecs().get<SkillBookComponent>(entity);
    for (auto& skill : book.skills)
    {
        SkillMod probe;
        probe.scope = static_cast<int32_t>(op.cdTarget == CdTarget::Bound              ? SkillScope::Bound
                                             : op.cdTarget == CdTarget::ArtifactSlots     ? SkillScope::ArtifactSlots
                                             : op.cdTarget == CdTarget::BaseSlotsCrazyPlus ? SkillScope::BaseSlotsCrazyPlus
                                             : op.cdTarget == CdTarget::Slots              ? SkillScope::Slots
                                                                                             : SkillScope::All);
        probe.slot  = op.i0;
        if (op.cdTarget == CdTarget::Slots || op.cdTarget == CdTarget::ArtifactSlots)
        {
            for (const int32_t slot : op.ids)
            {
                probe.slot = slot;
                if (skillInScope(book, skill, probe, bound))
                    fn(skill);
            }
            continue;
        }
        if (skillInScope(book, skill, probe, bound))
            fn(skill);
    }
}

// 配置槽位编号 → 技能池槽名（与技能簿装配一致）。
const char* cdSlotName(int32_t slot)
{
    switch (static_cast<EnumSkillSlot>(slot))
    {
    case EnumSkillSlot::Attack:
        return "A";
    case EnumSkillSlot::Sprint:
        return "Y";
    case EnumSkillSlot::Dodge:
        return "F";
    case EnumSkillSlot::Crazy:
        return "E";
    case EnumSkillSlot::A:
        return "B";
    case EnumSkillSlot::B:
        return "C";
    case EnumSkillSlot::C:
        return "D";
    case EnumSkillSlot::EA:
        return "G";
    case EnumSkillSlot::EB:
        return "H";
    case EnumSkillSlot::SUA:
        return "I";
    case EnumSkillSlot::SUB:
        return "J";
    case EnumSkillSlot::SUC:
        return "K";
    case EnumSkillSlot::Break:
        return "L";
    case EnumSkillSlot::Anger:
        return "P";
    default:
        return "";
    }
}

// 技能是否落在该配置槽上：直接比 slot，或按槽名映射。
bool cdSkillInSlot(const SkillBookComponent& book, const SkillRuntime& skill, int32_t slot)
{
    if (skill.slot == slot)
        return true;
    const auto name = skill.slot >= 0 && static_cast<size_t>(skill.slot) < book.slots.size()
                          ? book.slots[static_cast<size_t>(skill.slot)]
                          : std::string{};
    const char* mapped = cdSlotName(slot);
    return mapped[0] != '\0' && name == mapped;
}

// 按槽改 CD：先按随机/顺序取 n 个槽，再用对应阈值/基准/增量。
void applyPerSlotCd(World& world, Entity entity, const BuffOp& op)
{
    if (!world.ecs().has<SkillBookComponent>(entity) || op.ids.empty())
        return;
    auto& book          = world.ecs().get<SkillBookComponent>(entity);
    const int32_t total = static_cast<int32_t>(op.ids.size());
    int32_t n           = op.i0;
    if (n > total)
        n = total;
    if (n <= 0)
        return;
    std::vector<int32_t> order(static_cast<size_t>(total));
    for (int32_t i = 0; i < total; ++i)
        order[static_cast<size_t>(i)] = i;
    if (op.pick == PickMode::RandomOne)
    {
        for (int32_t i = 0; i < n; ++i)
        {
            const int32_t span = total - i;
            int32_t off        = 0;
            if (span > 1)
            {
                const int64_t draw = rng::draw(world, rng::DrawKind::Choice).toInt();
                off                = static_cast<int32_t>(draw % span);
                if (off < 0)
                    off = 0;
            }
            std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(i + off)]);
        }
    }
    for (int32_t i = 0; i < n; ++i)
    {
        const int32_t index       = order[static_cast<size_t>(i)];
        const int32_t slot        = op.ids[static_cast<size_t>(index)];
        const Fixed threshold     = static_cast<size_t>(index) < op.table.size()
                                        ? op.table[static_cast<size_t>(index)]
                                        : Fixed{};
        const int32_t base        = static_cast<size_t>(index) < op.ids2.size()
                                        ? op.ids2[static_cast<size_t>(index)]
                                        : 1;
        const Fixed amount        = static_cast<size_t>(index) < op.extra.size()
                                        ? op.extra[static_cast<size_t>(index)]
                                        : Fixed{};
        const Fixed absAmount     = amount < Fixed{} ? -amount : amount;
        for (auto& skill : book.skills)
        {
            if (!cdSkillInSlot(book, skill, slot))
                continue;
            const Fixed time = skill.cd;
            if (time == Fixed{})
                continue;
            const bool over  = threshold > Fixed{} && time > threshold;
            const bool under = threshold < Fixed{} && time < -threshold;
            if (!over && !under)
                continue;
            if (absAmount <= Fixed::fromInt(1))
            {
                const Fixed scale = Fixed::fromInt(1) + amount;
                if (base == 1)
                    skill.cd = skill.coldMaxTime * scale;
                else if (base == 2)
                    skill.cd = skill.cd * scale;
            }
            else
            {
                const Fixed delta = amount * Fixed::fromInt(1000);
                skill.cd += delta;
                skill.coldMaxTime += delta;
            }
            if (skill.cd < Fixed{})
                skill.cd = {};
            skill.timing = skill.cd > Fixed{};
        }
    }
}
}  // namespace

// 标准伤害。
Fixed standardHurt(World& world, Entity attacker)
{
    attacker = rootOf(world, attacker);
    if (!world.ecs().has<IdentityComponent>(attacker) || !world.ecs().has<StatsComponent>(attacker))
        return {};
    const auto& identity = world.ecs().get<IdentityComponent>(attacker);
    const auto& stats    = world.ecs().get<StatsComponent>(attacker);
    const bool hero     = identity.roleType == static_cast<int32_t>(mugen::EntityRoleType::kHero);
    const int32_t level = hero ? identity.level : std::max(1, identity.level / 10);
    const auto* row      = conf::Config::instance().find<conf::SkillHurtConfig>(level);
    if (!row)
        return {};
    return stats.value(hero ? 5 : 15) + Fixed::fromInt(hero ? row->hurt : row->monsterHurt);
}

// 无法移动时位移倍率清零；可移动时按移速修正重建。
void syncMoveScale(World& world, Entity entity)
{
    if (!world.ecs().has<DisplacementComponent>(entity))
        return;
    auto& scale = world.ecs().get<DisplacementComponent>(entity).velocityScale;
    if (world.ecs().has<StatusFlagsComponent>(entity) && !world.ecs().get<StatusFlagsComponent>(entity).canMove())
    {
        scale = {};
        return;
    }
    Fixed x = Fixed::fromInt(1);
    Fixed z = Fixed::fromInt(1);
    if (world.ecs().has<StatsComponent>(entity))
    {
        const auto& stats = world.ecs().get<StatsComponent>(entity);
        x = stats.finalStat(static_cast<int32_t>(BuffStat::MoveScaleX));
        z = stats.finalStat(static_cast<int32_t>(BuffStat::MoveScaleZ));
    }
    scale = {x, Fixed::fromInt(1), z};
}

// 撤销该实例登记的常驻效果。
void revoke(World& world, Entity entity, uint32_t serial, bool removeKids)
{
    if (world.ecs().has<StatsComponent>(entity))
    {
        auto& mods = world.ecs().get<StatsComponent>(entity).mods;
        std::erase_if(mods, [serial](const StatMod& mod) { return mod.source == serial; });
    }
    if (world.ecs().has<SkillBookComponent>(entity))
    {
        auto& mods = world.ecs().get<SkillBookComponent>(entity).mods;
        std::erase_if(mods, [serial](const SkillMod& mod) { return mod.source == serial; });
    }
    if (world.ecs().has<BuffListComponent>(entity))
    {
        auto& immune = world.ecs().get<BuffListComponent>(entity).immunity;
        std::erase_if(immune, [serial](const BuffImmune& row) { return row.source == serial; });
    }
    if (world.ecs().has<TauntTableComponent>(entity))
    {
        auto& entries = world.ecs().get<TauntTableComponent>(entity).entries;
        std::erase_if(entries, [serial](const TauntEntry& row) { return row.source == serial; });
    }
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->id) : nullptr;
    if (!def || !world.ecs().has<StatusFlagsComponent>(entity))
        return;
    auto& flags = world.ecs().get<StatusFlagsComponent>(entity);
    for (const auto& op : def->persistent)
    {
        if (op.effect != OpEffect::Active)
            continue;
        if (op.kind == BuffOpKind::Flag)
            flags.removeFlag(op.flag);
        else if (op.kind == BuffOpKind::Shield && world.ecs().has<HealthComponent>(entity))
            world.ecs().get<HealthComponent>(entity).shield = {};
        else if (op.kind == BuffOpKind::Visual)
            visualCue(world, entity, op.visual, 0);
        else if (op.kind == BuffOpKind::Kids && removeKids)
        {
            for (const int32_t id : op.ids)
                rules::remove(world, entity, id);
        }
    }
    syncMoveScale(world, entity);
}

// 执行一条 op。
void execOp(Exec& exec, const BuffOp& op)
{
    if (op.effect != OpEffect::Active)
        return;
    if (op.when.kind != PredKind::None && !evalPredicate(exec, op.when))
        return;
    if (op.onLatch && !exec.justLatched)
        return;
    auto& world = *exec.world;
    auto* inst  = findInstance(world, exec.self, exec.serial);
    if (!inst && !exec.fromAbility)
        return;
    const uint32_t serial = exec.serial;
    const Entity self     = exec.self;
    const Fixed value     = evalValue(exec, op.value);
    switch (op.kind)
    {
    case BuffOpKind::Stat:
        putStat(world, self, static_cast<int32_t>(op.stat), value, serial, op.live);
        if (op.stat == BuffStat::MoveScaleX || op.stat == BuffStat::MoveScaleZ)
            syncMoveScale(world, self);
        break;
    case BuffOpKind::Flag:
        if (world.ecs().has<StatusFlagsComponent>(self))
            world.ecs().get<StatusFlagsComponent>(self).addFlag(op.flag);
        if (op.flag == BuffFlag::Durance || op.flag == BuffFlag::Palsy || op.flag == BuffFlag::Stun ||
            op.flag == BuffFlag::Frozen)
            syncMoveScale(world, self);
        break;
    case BuffOpKind::SkillMod:
        if (world.ecs().has<SkillBookComponent>(self))
        {
            auto& book = world.ecs().get<SkillBookComponent>(self);
            if (op.scope == SkillScope::Slots || op.scope == SkillScope::ArtifactSlots)
            {
                for (const int32_t slot : op.ids)
                    book.mods.push_back(
                        {static_cast<int32_t>(op.field), static_cast<int32_t>(op.scope), slot, value, serial});
            }
            else
                book.mods.push_back(
                    {static_cast<int32_t>(op.field), static_cast<int32_t>(op.scope), op.i0, value, serial});
        }
        break;
    case BuffOpKind::Immune:
        if (world.ecs().has<BuffListComponent>(self))
        {
            auto& list = world.ecs().get<BuffListComponent>(self);
            if (op.immune == ImmuneSel::All)
                list.immunity.push_back({static_cast<int32_t>(ImmuneSel::All), 0, serial});
            else
                for (const int32_t key : op.ids)
                    list.immunity.push_back({static_cast<int32_t>(op.immune), key, serial});
        }
        break;
    case BuffOpKind::HpLock:
        putStat(world, self, kHpLockStat, value, serial, true);
        break;
    case BuffOpKind::Shield:
        if (world.ecs().has<HealthComponent>(self))
            world.ecs().get<HealthComponent>(self).shield = value;
        break;
    case BuffOpKind::Taunt:
        if (world.ecs().has<TauntTableComponent>(self))
            world.ecs().get<TauntTableComponent>(self).entries.push_back({inst->applicator, value, serial});
        break;
    case BuffOpKind::Kids:
        for (const int32_t id : op.ids)
        {
            BuffContext child;
            child.applicator = inst->applicator;
            child.boundSkill = inst->boundSkill;
            rules::admit(world, self, id, child);
        }
        break;
    case BuffOpKind::Visual:
        visualCue(world, self, op.visual, 1);
        break;
    case BuffOpKind::Dot:
    {
        const Entity root = rootOf(world, inst->applicator.value != 0 ? inst->applicator : self);
        Fixed addition{};
        if (world.ecs().has<IdentityComponent>(root))
            addition = std::max(Fixed{}, Fixed::fromInt(world.ecs().get<IdentityComponent>(root).level)) *
                       conf::kBuffHurtDefaultAddition;
        const Fixed hurt = (standardHurt(world, root) * (value * (Fixed::fromInt(1) + addition))).floor() *
                           Fixed::fromInt(std::max(1, inst->stacks));
        if (hurt != Fixed{} && world.ecs().has<HealthComponent>(self))
            health::rules::applyDelta(world, self, hurt, hurt < Fixed{} ? HpCause::Dot : HpCause::Heal,
                                      inst->applicator, {});
        break;
    }
    case BuffOpKind::Resource:
    {
        if (op.res == ResourceId::Hp && world.ecs().has<HealthComponent>(self))
        {
            const HpCause cause = value >= Fixed{} ? HpCause::Heal : HpCause::Dot;
            health::rules::applyDelta(world, self, value, cause, inst->applicator, {}, op.ignoreShield);
        }
        else if (op.res == ResourceId::Mp && world.ecs().has<HealthComponent>(self))
        {
            auto& health = world.ecs().get<HealthComponent>(self);
            health.mp  = std::clamp(health.mp + value, Fixed{}, health.maxMp);
        }
        else if (op.res == ResourceId::Ep && world.ecs().has<HealthComponent>(self))
        {
            auto& health = world.ecs().get<HealthComponent>(self);
            health.ep  = std::clamp(health.ep + value, Fixed{}, health.maxEp);
        }
        else if (op.res == ResourceId::Anger && world.ecs().has<RageStateComponent>(self))
        {
            auto& rage = world.ecs().get<RageStateComponent>(self);
            if (op.transformFreeze && rage.transformed)
                rage.freezeMs = std::clamp(rage.freezeMs + value, Fixed{}, rage.maxFreezeMs);
            else
                rage.rage = std::max(Fixed{}, rage.rage + value);
        }
        break;
    }
    case BuffOpKind::Apply:
    {
        inst                = findInstance(world, self, serial);
        if (!inst)
            break;
        int32_t copies = 1;
        if (op.value.kind != ValueKind::None)
            copies = std::max(1, static_cast<int32_t>(evalValue(exec, op.value).toInt()));
        std::vector<int32_t> ids = op.ids;
        if (op.pick == PickMode::Weighted || op.pick == PickMode::RandomOne)
        {
            const int32_t index = op.pick == PickMode::Weighted ? pickWeighted(world, op.table)
                                                                  : static_cast<int32_t>(rng::draw(world, rng::DrawKind::Choice).toInt()) %
                                                                        std::max(1, static_cast<int32_t>(ids.size()));
            if (!ids.empty())
                ids = {ids[static_cast<size_t>(std::clamp(index, 0, static_cast<int32_t>(ids.size()) - 1))]};
        }
        else if (op.pick == PickMode::Sequential && !ids.empty())
        {
            const int32_t index = inst->cursor % static_cast<int32_t>(ids.size());
            inst->cursor++;
            ids = {ids[static_cast<size_t>(index)]};
        }
        for (int32_t n = 0; n < copies; ++n)
            for (const int32_t id : ids)
            {
                const auto* child = buffDef(id);
                int32_t selector  = 1;
                if (op.to == ApplyTo::Applicator)
                    selector = 0;
                else if (op.to == ApplyTo::EventTarget)
                    selector = 2;
                else if (op.to == ApplyTo::Self)
                    selector = 1;
                else if (op.to == ApplyTo::Area)
                    selector = op.i0;
                else if (child)
                    selector = child->admission.target == 0 ? 1 : child->admission.target;
                std::vector<Entity> targets;
                if (selector == 0)
                    targets = {inst->applicator};
                else
                    targets = selectTargets(exec, selector);
                for (Entity target : targets)
                {
                    if (op.to == ApplyTo::Area && !inArea(world, self, target, op.f0, op.f1))
                        continue;
                    if (!world.ecs().alive(target) || !world.ecs().has<BuffListComponent>(target))
                        continue;
                    BuffContext ctx;
                    ctx.applicator = inst->applicator.value != 0 ? inst->applicator : self;
                    ctx.boundSkill = inst->boundSkill;
                    ctx.target     = inst->eventTarget;
                    rules::admit(world, target, id, ctx);
                }
            }
        break;
    }
    case BuffOpKind::Remove:
    {
        int32_t removed = 0;
        if (!world.ecs().has<BuffListComponent>(self))
            break;
        std::vector<uint32_t> serials;
        for (const auto& row : world.ecs().get<BuffListComponent>(self).instances)
        {
            if (row.removing)
                continue;
            const auto* def = buffDef(row.id);
            bool match      = false;
            if (op.by == RemoveBy::Id)
                match = std::find(op.ids.begin(), op.ids.end(), row.id) != op.ids.end();
            else if (op.by == RemoveBy::Rule && def)
                match = std::find(op.ids.begin(), op.ids.end(), def->admission.ruleId) != op.ids.end();
            else if (op.by == RemoveBy::Type && def)
                match = std::find(op.ids.begin(), op.ids.end(), def->admission.buffType) != op.ids.end();
            if (match)
                serials.push_back(row.serial);
        }
        for (const uint32_t id : serials)
        {
            destroySerial(world, self, id);
            ++removed;
        }
        // 实际全删；寄存器按上限封顶（规则 401 的 p2）。
        exec.reg = Fixed::fromInt(op.i0 > 0 && removed > op.i0 ? op.i0 : removed);
        break;
    }
    case BuffOpKind::Count:
    {
        int32_t count = 0;
        if (world.ecs().has<BuffListComponent>(self))
            for (const auto& row : world.ecs().get<BuffListComponent>(self).instances)
            {
                if (row.removing)
                    continue;
                const auto* def = buffDef(row.id);
                bool match      = false;
                if (op.countBy == CountBy::Id)
                    match = row.id == op.i0;
                else if (op.countBy == CountBy::Rule && def)
                    match = def->admission.ruleId == op.i0;
                else if (op.countBy == CountBy::Type && def)
                    match = def->admission.buffType == op.i0;
                if (!match)
                    continue;
                const bool stacks = op.stackCmp == StackCmp::None ||
                                    (op.stackCmp == StackCmp::AtLeast && row.stacks >= op.i1) ||
                                    (op.stackCmp == StackCmp::Equal && row.stacks == op.i1) ||
                                    (op.stackCmp == StackCmp::NotEqual && row.stacks != op.i1) ||
                                    (op.stackCmp == StackCmp::Below && row.stacks < op.i1);
                if (stacks)
                    ++count;
            }
        exec.reg = Fixed::fromInt(count);
        break;
    }
    case BuffOpKind::Cooldown:
    {
        if (op.cdMode == CdMode::PerSlot)
        {
            applyPerSlotCd(world, self, op);
            break;
        }
        eachCdSkill(world, self, op, inst->boundSkill, [&](SkillRuntime& skill) {
            if (op.cdMode == CdMode::Reset)
            {
                skill.cd     = {};
                skill.timing = false;
            }
            else if (op.cdMode == CdMode::SetRatio)
                skill.cd = skill.coldMaxTime * value;
            else if (op.cdMode == CdMode::Add)
                skill.cd += value * Fixed::fromInt(1000);
            else if (op.cdMode == CdMode::Reduce)
                skill.cd = std::max(Fixed{}, skill.cd - value * Fixed::fromInt(1000));
        });
        break;
    }
    case BuffOpKind::Extend:
        if (world.ecs().has<BuffListComponent>(self))
            for (auto& row : world.ecs().get<BuffListComponent>(self).instances)
            {
                const auto* def = buffDef(row.id);
                if (def && def->admission.ruleId == op.i0)
                {
                    row.maxTimes += op.i1;
                    break;
                }
            }
        break;
    case BuffOpKind::Ability:
        if (world.ecs().has<AbilityListComponent>(self))
        {
            const int32_t abilityId = op.i0 != 0 ? op.i0 : (op.ids.empty() ? 0 : op.ids.front());
            const auto* ability     = conf::Config::instance().find<AbilityDef>(abilityId);
            if (ability && ability->status != LowerStatus::Unsupported)
            {
                auto& list = world.ecs().get<AbilityListComponent>(self);
                AbilityInstance* slot = nullptr;
                if (ability->subType != -1)
                    for (auto& row : list.instances)
                        if (row.id == ability->id)
                            slot = &row;
                if (!slot)
                {
                    AbilityInstance added;
                    added.serial     = list.nextSerial++;
                    added.id         = ability->id;
                    added.endLeft    = ability->endedType == 1 ? ability->endedValue : Fixed::fromInt(-1);
                    added.applicator = inst ? inst->applicator : self;
                    list.instances.push_back(added);
                    slot = &list.instances.back();
                }
                if (std::find(ability->events.begin(), ability->events.end(), TriggerPoint::InitAbility) !=
                    ability->events.end())
                    triggerAbility(world, self, *slot, *ability);
            }
        }
        break;
    case BuffOpKind::SpawnEffect:
        for (const int32_t id : op.ids)
        {
            SourceContext source;
            source.skillId = inst ? inst->skillId : -1;
            effect::rules::spawnFromRole(world, self, id, source);
        }
        break;
    case BuffOpKind::EnterCrazy:
        if (world.ecs().has<SkillCastComponent>(self))
            world.ecs().get<SkillCastComponent>(self).crazy = true;
        break;
    case BuffOpKind::Interrupt:
        if (world.ecs().has<SkillCastComponent>(self))
            skill::rules::forceInterrupt(world, self);
        break;
    case BuffOpKind::SetCost:
        break;
    case BuffOpKind::Sound:
        if (op.i0 > 0)
        {
            CueEvent event;
            event.entity = self;
            event.kind   = CueKind::Sound;
            event.id     = op.i0;
            world.ecs().singleton<EventQueue<CueEvent>>().push(event);
        }
        break;
    case BuffOpKind::ExitSelf:
        destroySerial(world, self, serial);
        break;
    case BuffOpKind::Choice:
    {
        if (inst->choice < 0)
        {
            int32_t count = static_cast<int32_t>(op.ids2.size());
            inst->choice = count > 0 ? pickWeighted(world, op.table) % count : 0;
        }
        const auto* def = buffDef(inst->id);
        if (!def || inst->choice < 0 || static_cast<size_t>(inst->choice) >= op.ids2.size())
            break;
        int32_t begin = op.choiceBegin;
        for (int32_t i = 0; i < inst->choice; ++i)
            begin += op.ids2[static_cast<size_t>(i)];
        const int32_t length = op.ids2[static_cast<size_t>(inst->choice)];
        for (int32_t i = 0; i < length; ++i)
        {
            const size_t index = static_cast<size_t>(begin + i);
            if (index < def->choicePool.size())
                execOp(exec, def->choicePool[index]);
        }
        break;
    }
    case BuffOpKind::SummonLink:
    case BuffOpKind::OnSummonDeath:
    case BuffOpKind::Watch:
    case BuffOpKind::StateTimer:
    case BuffOpKind::None:
        break;
    }
}

// 一组 op 共用一个从零开始的寄存器。
void runOps(Exec& exec, const std::vector<BuffOp>& ops)
{
    const Fixed saved = exec.reg;
    exec.reg        = {};
    for (const auto& op : ops)
    {
        if (!exec.fromAbility && !findInstance(*exec.world, exec.self, exec.serial))
            break;
        execOp(exec, op);
    }
    exec.reg = saved;
}

// 特殊能力的一次触发：条件、概率、冷却，然后执行降级 op。
void triggerAbility(World& world, Entity entity, AbilityInstance& ability, const AbilityDef& def)
{
    if (def.status != LowerStatus::Lowered)
        return;
    if (ability.cdLeft > Fixed{})
        return;
    Exec exec;
    exec.world       = &world;
    exec.self        = entity;
    exec.fromAbility = true;
    for (const auto& pred : def.conditions)
        if (!evalPredicate(exec, pred))
            return;
    if (def.probability < Fixed::fromInt(100))
    {
        const Fixed draw = rng::draw(world, rng::DrawKind::AbilityProb);
        if (draw > def.probability)
            return;
    }
    ability.cdLeft = world.setup().pvp ? def.cdPvp : def.cd;
    runOps(exec, def.ops);
}

// 钩子：首次合闩时先跑写入 r0 的瞬时 op，再合常驻，最后跑其余瞬时；已合闩则按需重施常驻后再瞬时。
void onHook(World& world, Entity entity, uint32_t serial, int32_t hook)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->id) : nullptr;
    if (!inst || !def)
        return;
    const bool was      = inst->latched;
    const bool latchNow = !was && (def->activation.latchOn & hook) != 0;
    Exec exec;
    exec.world  = &world;
    exec.self   = entity;
    exec.serial = serial;
    exec.hook   = hook;
    const std::vector<BuffOp>* list = nullptr;
    if (hook == static_cast<int32_t>(BuffHook::Enter))
        list = &def->onEnter;
    else if (hook == static_cast<int32_t>(BuffHook::Tick))
        list = &def->onTick;
    else if (hook == static_cast<int32_t>(BuffHook::Begin))
        list = &def->onBegin;
    else if (hook == static_cast<int32_t>(BuffHook::End))
        list = &def->onEnd;
    else if (hook == static_cast<int32_t>(BuffHook::Exit))
        list = &def->onExit;
    auto stillHere = [&]() {
        return findInstance(*exec.world, exec.self, exec.serial) != nullptr;
    };
    auto writesReg = [](BuffOpKind kind) {
        return kind == BuffOpKind::Remove || kind == BuffOpKind::Count;
    };
    if (latchNow)
    {
        inst->latched    = true;
        exec.justLatched = true;
        // Remove/Count 先写入 r0，常驻 Stat(Reg) 才能读到；其余瞬时仍在常驻之后。
        if (list)
            for (const auto& op : *list)
            {
                if (!stillHere())
                    return;
                if (writesReg(op.kind))
                    execOp(exec, op);
            }
        if (!stillHere())
            return;
        for (const auto& op : def->persistent)
        {
            if (!stillHere())
                return;
            execOp(exec, op);
        }
        if (list)
            for (const auto& op : *list)
            {
                if (!stillHere())
                    return;
                if (!writesReg(op.kind))
                    execOp(exec, op);
            }
    }
    else
    {
        if (was && (def->activation.latchOn & hook) != 0)
        {
            for (const auto& op : def->persistent)
            {
                if (op.reapply ||
                    (op.kind == BuffOpKind::Kids && (hook & static_cast<int32_t>(BuffHook::Tick)) != 0) ||
                    (op.kind == BuffOpKind::Stat && op.live))
                    execOp(exec, op);
            }
        }
        inst = findInstance(world, entity, serial);
        if (!inst)
            return;
        if (list)
            runOps(exec, *list);
    }
    inst = findInstance(world, entity, serial);
    if (!inst)
        return;
    if ((def->activation.releaseOn & hook) != 0 && inst->latched)
    {
        inst->latched = false;
        revoke(world, entity, serial);
    }
}

// 立即删除。退出钩子里若再次删除，序号已经不在。
void destroySerial(World& world, Entity entity, uint32_t serial)
{
    auto* inst = findInstance(world, entity, serial);
    if (!inst || !world.ecs().has<BuffListComponent>(entity))
        return;
    if (inst->removing)
        return;
    inst->removing = true;
    const int32_t id = inst->id;
    onHook(world, entity, serial, static_cast<int32_t>(BuffHook::Exit));
    inst = findInstance(world, entity, serial);
    if (inst && inst->latched)
    {
        inst->latched = false;
        // 退出钩子没有撤销时，仍撤销属性与标记，但留下已经加上的子 Buff。
        revoke(world, entity, serial, false);
    }
    auto& list = world.ecs().get<BuffListComponent>(entity);
    std::erase_if(list.instances, [serial](const BuffInstance& row) { return row.serial == serial; });
    BuffEvent event;
    event.entity = entity;
    event.buffId = id;
    event.serial = serial;
    event.phase  = BuffPhase::Removed;
    world.ecs().singleton<EventQueue<BuffEvent>>().push(event);
    BuffContext ctx;
    ctx.target = entity;
    rules::fire(world, entity, TriggerPoint::RemoveBuff, ctx);
}
}  // namespace mugen::sim::buff::detail
