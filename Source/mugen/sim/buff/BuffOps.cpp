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
    if (world.ecs().alive(identity.m_rootOwner) && world.ecs().has<StatsComponent>(identity.m_rootOwner))
        return identity.m_rootOwner;
    if (world.ecs().alive(identity.m_owner) && world.ecs().has<StatsComponent>(identity.m_owner))
        return identity.m_owner;
    return entity;
}

// 登记或替换一条属性修正。
void putStat(World& world, Entity entity, int32_t stat, Fixed value, uint32_t source, bool replace)
{
    if (!world.ecs().has<StatsComponent>(entity))
        return;
    auto& mods = world.ecs().get<StatsComponent>(entity).m_mods;
    if (replace)
    {
        for (auto& mod : mods)
            if (mod.m_source == source && mod.m_stat == stat)
            {
                mod.m_value = value;
                return;
            }
    }
    mods.push_back({stat, value, source});
}

// 技能修正是否作用到这一条技能。
bool skillInScope(const SkillBookComponent& book, const SkillRuntime& skill, const SkillMod& mod, int32_t bound)
{
    const auto name = skill.m_slot >= 0 && static_cast<size_t>(skill.m_slot) < book.m_slots.size()
                          ? book.m_slots[static_cast<size_t>(skill.m_slot)]
                          : std::string{};
    switch (static_cast<SkillScope>(mod.m_scope))
    {
    case SkillScope::Bound:
        return bound > 0 && skill.m_configId == bound;
    case SkillScope::Slots:
    case SkillScope::ArtifactSlots:
        return mod.m_slot == skill.m_slot;
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
    event.m_entity = entity;
    event.m_kind   = cue;
    event.m_param  = on;
    world.ecs().singleton<EventQueue<CueEvent>>().push(event);
}

// 阵营关系。
bool hostile(const IdentityComponent& self, const IdentityComponent& other)
{
    return (static_cast<uint32_t>(self.m_hostileMask) & static_cast<uint32_t>(other.m_camp)) != 0;
}

bool sameCamp(const IdentityComponent& self, const IdentityComponent& other)
{
    return self.m_camp == other.m_camp;
}

// 按选择器收集角色。
std::vector<Entity> selectTargets(Exec& exec, int32_t selector)
{
    auto& world = *exec.m_world;
    auto* inst  = findInstance(world, exec.m_self, exec.m_serial);
    const Entity eventTarget = inst ? inst->m_eventTarget : Entity{};
    const Entity self        = exec.m_self;
    std::vector<Entity> out;
    if (!world.ecs().has<IdentityComponent>(self))
        return out;
    const auto& selfId = world.ecs().get<IdentityComponent>(self);
    auto accept        = [&](Entity entity, const IdentityComponent& identity) {
        if (world.ecs().has<InactiveComponent>(entity) || world.ecs().has<DespawningComponent>(entity))
            return;
        if (identity.m_kind != SpawnKind::Role)
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
            if (identity.m_roleType == static_cast<int32_t>(mugen::EntityRoleType::kBoss))
                out.push_back(entity);
            break;
        case 7:
            if (identity.m_roleType == static_cast<int32_t>(mugen::EntityRoleType::kMonster) ||
                identity.m_roleType == static_cast<int32_t>(mugen::EntityRoleType::kElite))
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
    const auto& a = world.ecs().get<TransformComponent>(origin).m_position;
    const auto& b = world.ecs().get<TransformComponent>(other).m_position;
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
    for (auto& skill : book.m_skills)
    {
        SkillMod probe;
        probe.m_scope = static_cast<int32_t>(op.m_cdTarget == CdTarget::Bound              ? SkillScope::Bound
                                             : op.m_cdTarget == CdTarget::ArtifactSlots     ? SkillScope::ArtifactSlots
                                             : op.m_cdTarget == CdTarget::BaseSlotsCrazyPlus ? SkillScope::BaseSlotsCrazyPlus
                                             : op.m_cdTarget == CdTarget::Slots              ? SkillScope::Slots
                                                                                             : SkillScope::All);
        probe.m_slot  = op.m_i0;
        if (op.m_cdTarget == CdTarget::Slots || op.m_cdTarget == CdTarget::ArtifactSlots)
        {
            for (const int32_t slot : op.m_ids)
            {
                probe.m_slot = slot;
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

// 技能是否落在该配置槽上：直接比 m_slot，或按槽名映射。
bool cdSkillInSlot(const SkillBookComponent& book, const SkillRuntime& skill, int32_t slot)
{
    if (skill.m_slot == slot)
        return true;
    const auto name = skill.m_slot >= 0 && static_cast<size_t>(skill.m_slot) < book.m_slots.size()
                          ? book.m_slots[static_cast<size_t>(skill.m_slot)]
                          : std::string{};
    const char* mapped = cdSlotName(slot);
    return mapped[0] != '\0' && name == mapped;
}

// 按槽改 CD：先按随机/顺序取 n 个槽，再用对应阈值/基准/增量。
void applyPerSlotCd(World& world, Entity entity, const BuffOp& op)
{
    if (!world.ecs().has<SkillBookComponent>(entity) || op.m_ids.empty())
        return;
    auto& book          = world.ecs().get<SkillBookComponent>(entity);
    const int32_t total = static_cast<int32_t>(op.m_ids.size());
    int32_t n           = op.m_i0;
    if (n > total)
        n = total;
    if (n <= 0)
        return;
    std::vector<int32_t> order(static_cast<size_t>(total));
    for (int32_t i = 0; i < total; ++i)
        order[static_cast<size_t>(i)] = i;
    if (op.m_pick == PickMode::RandomOne)
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
        const int32_t slot        = op.m_ids[static_cast<size_t>(index)];
        const Fixed threshold     = static_cast<size_t>(index) < op.m_table.size()
                                        ? op.m_table[static_cast<size_t>(index)]
                                        : Fixed{};
        const int32_t base        = static_cast<size_t>(index) < op.m_ids2.size()
                                        ? op.m_ids2[static_cast<size_t>(index)]
                                        : 1;
        const Fixed amount        = static_cast<size_t>(index) < op.m_extra.size()
                                        ? op.m_extra[static_cast<size_t>(index)]
                                        : Fixed{};
        const Fixed absAmount     = amount < Fixed{} ? -amount : amount;
        for (auto& skill : book.m_skills)
        {
            if (!cdSkillInSlot(book, skill, slot))
                continue;
            const Fixed time = skill.m_cd;
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
                    skill.m_cd = skill.m_coldMaxTime * scale;
                else if (base == 2)
                    skill.m_cd = skill.m_cd * scale;
            }
            else
            {
                const Fixed delta = amount * Fixed::fromInt(1000);
                skill.m_cd += delta;
                skill.m_coldMaxTime += delta;
            }
            if (skill.m_cd < Fixed{})
                skill.m_cd = {};
            skill.m_timing = skill.m_cd > Fixed{};
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
    const bool hero     = identity.m_roleType == static_cast<int32_t>(mugen::EntityRoleType::kHero);
    const int32_t level = hero ? identity.m_level : std::max(1, identity.m_level / 10);
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
    auto& scale = world.ecs().get<DisplacementComponent>(entity).m_velocityScale;
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
        auto& mods = world.ecs().get<StatsComponent>(entity).m_mods;
        std::erase_if(mods, [serial](const StatMod& mod) { return mod.m_source == serial; });
    }
    if (world.ecs().has<SkillBookComponent>(entity))
    {
        auto& mods = world.ecs().get<SkillBookComponent>(entity).m_mods;
        std::erase_if(mods, [serial](const SkillMod& mod) { return mod.m_source == serial; });
    }
    if (world.ecs().has<BuffListComponent>(entity))
    {
        auto& immune = world.ecs().get<BuffListComponent>(entity).m_immunity;
        std::erase_if(immune, [serial](const BuffImmune& row) { return row.m_source == serial; });
    }
    if (world.ecs().has<TauntTableComponent>(entity))
    {
        auto& entries = world.ecs().get<TauntTableComponent>(entity).m_entries;
        std::erase_if(entries, [serial](const TauntEntry& row) { return row.m_source == serial; });
    }
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->m_id) : nullptr;
    if (!def || !world.ecs().has<StatusFlagsComponent>(entity))
        return;
    auto& flags = world.ecs().get<StatusFlagsComponent>(entity);
    for (const auto& op : def->m_persistent)
    {
        if (op.m_effect != OpEffect::Active)
            continue;
        if (op.m_kind == BuffOpKind::Flag)
            flags.removeFlag(op.m_flag);
        else if (op.m_kind == BuffOpKind::Shield && world.ecs().has<HealthComponent>(entity))
            world.ecs().get<HealthComponent>(entity).m_shield = {};
        else if (op.m_kind == BuffOpKind::Visual)
            visualCue(world, entity, op.m_visual, 0);
        else if (op.m_kind == BuffOpKind::Kids && removeKids)
        {
            for (const int32_t id : op.m_ids)
                rules::remove(world, entity, id);
        }
    }
    syncMoveScale(world, entity);
}

// 执行一条 op。
void execOp(Exec& exec, const BuffOp& op)
{
    if (op.m_effect != OpEffect::Active)
        return;
    if (op.m_when.m_kind != PredKind::None && !evalPredicate(exec, op.m_when))
        return;
    if (op.m_onLatch && !exec.m_justLatched)
        return;
    auto& world = *exec.m_world;
    auto* inst  = findInstance(world, exec.m_self, exec.m_serial);
    if (!inst && !exec.m_fromAbility)
        return;
    const uint32_t serial = exec.m_serial;
    const Entity self     = exec.m_self;
    const Fixed value     = evalValue(exec, op.m_value);
    switch (op.m_kind)
    {
    case BuffOpKind::Stat:
        putStat(world, self, static_cast<int32_t>(op.m_stat), value, serial, op.m_live);
        if (op.m_stat == BuffStat::MoveScaleX || op.m_stat == BuffStat::MoveScaleZ)
            syncMoveScale(world, self);
        break;
    case BuffOpKind::Flag:
        if (world.ecs().has<StatusFlagsComponent>(self))
            world.ecs().get<StatusFlagsComponent>(self).addFlag(op.m_flag);
        if (op.m_flag == BuffFlag::Durance || op.m_flag == BuffFlag::Palsy || op.m_flag == BuffFlag::Stun ||
            op.m_flag == BuffFlag::Frozen)
            syncMoveScale(world, self);
        break;
    case BuffOpKind::SkillMod:
        if (world.ecs().has<SkillBookComponent>(self))
        {
            auto& book = world.ecs().get<SkillBookComponent>(self);
            if (op.m_scope == SkillScope::Slots || op.m_scope == SkillScope::ArtifactSlots)
            {
                for (const int32_t slot : op.m_ids)
                    book.m_mods.push_back(
                        {static_cast<int32_t>(op.m_field), static_cast<int32_t>(op.m_scope), slot, value, serial});
            }
            else
                book.m_mods.push_back(
                    {static_cast<int32_t>(op.m_field), static_cast<int32_t>(op.m_scope), op.m_i0, value, serial});
        }
        break;
    case BuffOpKind::Immune:
        if (world.ecs().has<BuffListComponent>(self))
        {
            auto& list = world.ecs().get<BuffListComponent>(self);
            if (op.m_immune == ImmuneSel::All)
                list.m_immunity.push_back({static_cast<int32_t>(ImmuneSel::All), 0, serial});
            else
                for (const int32_t key : op.m_ids)
                    list.m_immunity.push_back({static_cast<int32_t>(op.m_immune), key, serial});
        }
        break;
    case BuffOpKind::HpLock:
        putStat(world, self, kHpLockStat, value, serial, true);
        break;
    case BuffOpKind::Shield:
        if (world.ecs().has<HealthComponent>(self))
            world.ecs().get<HealthComponent>(self).m_shield = value;
        break;
    case BuffOpKind::Taunt:
        if (world.ecs().has<TauntTableComponent>(self))
            world.ecs().get<TauntTableComponent>(self).m_entries.push_back({inst->m_applicator, value, serial});
        break;
    case BuffOpKind::Kids:
        for (const int32_t id : op.m_ids)
        {
            BuffContext child;
            child.m_applicator = inst->m_applicator;
            child.m_boundSkill = inst->m_boundSkill;
            rules::admit(world, self, id, child);
        }
        break;
    case BuffOpKind::Visual:
        visualCue(world, self, op.m_visual, 1);
        break;
    case BuffOpKind::Dot:
    {
        const Entity root = rootOf(world, inst->m_applicator.value != 0 ? inst->m_applicator : self);
        Fixed addition{};
        if (world.ecs().has<IdentityComponent>(root))
            addition = std::max(Fixed{}, Fixed::fromInt(world.ecs().get<IdentityComponent>(root).m_level)) *
                       conf::kBuffHurtDefaultAddition;
        const Fixed hurt = (standardHurt(world, root) * (value * (Fixed::fromInt(1) + addition))).floor() *
                           Fixed::fromInt(std::max(1, inst->m_stacks));
        if (hurt != Fixed{} && world.ecs().has<HealthComponent>(self))
            health::rules::applyDelta(world, self, hurt, hurt < Fixed{} ? HpCause::Dot : HpCause::Heal,
                                      inst->m_applicator, {});
        break;
    }
    case BuffOpKind::Resource:
    {
        if (op.m_res == ResourceId::Hp && world.ecs().has<HealthComponent>(self))
        {
            const HpCause cause = value >= Fixed{} ? HpCause::Heal : HpCause::Dot;
            health::rules::applyDelta(world, self, value, cause, inst->m_applicator, {}, op.m_ignoreShield);
        }
        else if (op.m_res == ResourceId::Mp && world.ecs().has<HealthComponent>(self))
        {
            auto& health = world.ecs().get<HealthComponent>(self);
            health.m_mp  = std::clamp(health.m_mp + value, Fixed{}, health.m_maxMp);
        }
        else if (op.m_res == ResourceId::Ep && world.ecs().has<HealthComponent>(self))
        {
            auto& health = world.ecs().get<HealthComponent>(self);
            health.m_ep  = std::clamp(health.m_ep + value, Fixed{}, health.m_maxEp);
        }
        else if (op.m_res == ResourceId::Anger && world.ecs().has<RageStateComponent>(self))
        {
            auto& rage = world.ecs().get<RageStateComponent>(self);
            if (op.m_transformFreeze && rage.m_transformed)
                rage.m_freezeMs = std::clamp(rage.m_freezeMs + value, Fixed{}, rage.m_maxFreezeMs);
            else
                rage.m_rage = std::max(Fixed{}, rage.m_rage + value);
        }
        break;
    }
    case BuffOpKind::Apply:
    {
        inst                = findInstance(world, self, serial);
        if (!inst)
            break;
        int32_t copies = 1;
        if (op.m_value.m_kind != ValueKind::None)
            copies = std::max(1, static_cast<int32_t>(evalValue(exec, op.m_value).toInt()));
        std::vector<int32_t> ids = op.m_ids;
        if (op.m_pick == PickMode::Weighted || op.m_pick == PickMode::RandomOne)
        {
            const int32_t index = op.m_pick == PickMode::Weighted ? pickWeighted(world, op.m_table)
                                                                  : static_cast<int32_t>(rng::draw(world, rng::DrawKind::Choice).toInt()) %
                                                                        std::max(1, static_cast<int32_t>(ids.size()));
            if (!ids.empty())
                ids = {ids[static_cast<size_t>(std::clamp(index, 0, static_cast<int32_t>(ids.size()) - 1))]};
        }
        else if (op.m_pick == PickMode::Sequential && !ids.empty())
        {
            const int32_t index = inst->m_cursor % static_cast<int32_t>(ids.size());
            inst->m_cursor++;
            ids = {ids[static_cast<size_t>(index)]};
        }
        for (int32_t n = 0; n < copies; ++n)
            for (const int32_t id : ids)
            {
                const auto* child = buffDef(id);
                int32_t selector  = 1;
                if (op.m_to == ApplyTo::Applicator)
                    selector = 0;
                else if (op.m_to == ApplyTo::EventTarget)
                    selector = 2;
                else if (op.m_to == ApplyTo::Self)
                    selector = 1;
                else if (op.m_to == ApplyTo::Area)
                    selector = op.m_i0;
                else if (child)
                    selector = child->m_admission.m_target == 0 ? 1 : child->m_admission.m_target;
                std::vector<Entity> targets;
                if (selector == 0)
                    targets = {inst->m_applicator};
                else
                    targets = selectTargets(exec, selector);
                for (Entity target : targets)
                {
                    if (op.m_to == ApplyTo::Area && !inArea(world, self, target, op.m_f0, op.m_f1))
                        continue;
                    if (!world.ecs().alive(target) || !world.ecs().has<BuffListComponent>(target))
                        continue;
                    BuffContext ctx;
                    ctx.m_applicator = inst->m_applicator.value != 0 ? inst->m_applicator : self;
                    ctx.m_boundSkill = inst->m_boundSkill;
                    ctx.m_target     = inst->m_eventTarget;
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
        for (const auto& row : world.ecs().get<BuffListComponent>(self).m_instances)
        {
            if (row.m_removing)
                continue;
            const auto* def = buffDef(row.m_id);
            bool match      = false;
            if (op.m_by == RemoveBy::Id)
                match = std::find(op.m_ids.begin(), op.m_ids.end(), row.m_id) != op.m_ids.end();
            else if (op.m_by == RemoveBy::Rule && def)
                match = std::find(op.m_ids.begin(), op.m_ids.end(), def->m_admission.m_ruleId) != op.m_ids.end();
            else if (op.m_by == RemoveBy::Type && def)
                match = std::find(op.m_ids.begin(), op.m_ids.end(), def->m_admission.m_buffType) != op.m_ids.end();
            if (match)
                serials.push_back(row.m_serial);
        }
        for (const uint32_t id : serials)
        {
            destroySerial(world, self, id);
            ++removed;
        }
        // 实际全删；寄存器按上限封顶（规则 401 的 p2）。
        exec.m_reg = Fixed::fromInt(op.m_i0 > 0 && removed > op.m_i0 ? op.m_i0 : removed);
        break;
    }
    case BuffOpKind::Count:
    {
        int32_t count = 0;
        if (world.ecs().has<BuffListComponent>(self))
            for (const auto& row : world.ecs().get<BuffListComponent>(self).m_instances)
            {
                if (row.m_removing)
                    continue;
                const auto* def = buffDef(row.m_id);
                bool match      = false;
                if (op.m_countBy == CountBy::Id)
                    match = row.m_id == op.m_i0;
                else if (op.m_countBy == CountBy::Rule && def)
                    match = def->m_admission.m_ruleId == op.m_i0;
                else if (op.m_countBy == CountBy::Type && def)
                    match = def->m_admission.m_buffType == op.m_i0;
                if (!match)
                    continue;
                const bool stacks = op.m_stackCmp == StackCmp::None ||
                                    (op.m_stackCmp == StackCmp::AtLeast && row.m_stacks >= op.m_i1) ||
                                    (op.m_stackCmp == StackCmp::Equal && row.m_stacks == op.m_i1) ||
                                    (op.m_stackCmp == StackCmp::NotEqual && row.m_stacks != op.m_i1) ||
                                    (op.m_stackCmp == StackCmp::Below && row.m_stacks < op.m_i1);
                if (stacks)
                    ++count;
            }
        exec.m_reg = Fixed::fromInt(count);
        break;
    }
    case BuffOpKind::Cooldown:
    {
        if (op.m_cdMode == CdMode::PerSlot)
        {
            applyPerSlotCd(world, self, op);
            break;
        }
        eachCdSkill(world, self, op, inst->m_boundSkill, [&](SkillRuntime& skill) {
            if (op.m_cdMode == CdMode::Reset)
            {
                skill.m_cd     = {};
                skill.m_timing = false;
            }
            else if (op.m_cdMode == CdMode::SetRatio)
                skill.m_cd = skill.m_coldMaxTime * value;
            else if (op.m_cdMode == CdMode::Add)
                skill.m_cd += value * Fixed::fromInt(1000);
            else if (op.m_cdMode == CdMode::Reduce)
                skill.m_cd = std::max(Fixed{}, skill.m_cd - value * Fixed::fromInt(1000));
        });
        break;
    }
    case BuffOpKind::Extend:
        if (world.ecs().has<BuffListComponent>(self))
            for (auto& row : world.ecs().get<BuffListComponent>(self).m_instances)
            {
                const auto* def = buffDef(row.m_id);
                if (def && def->m_admission.m_ruleId == op.m_i0)
                {
                    row.m_maxTimes += op.m_i1;
                    break;
                }
            }
        break;
    case BuffOpKind::Ability:
        if (world.ecs().has<AbilityListComponent>(self))
        {
            const int32_t abilityId = op.m_i0 != 0 ? op.m_i0 : (op.m_ids.empty() ? 0 : op.m_ids.front());
            const auto* ability     = conf::Config::instance().find<AbilityDef>(abilityId);
            if (ability && ability->m_status != LowerStatus::Unsupported)
            {
                auto& list = world.ecs().get<AbilityListComponent>(self);
                AbilityInstance* slot = nullptr;
                if (ability->m_subType != -1)
                    for (auto& row : list.m_instances)
                        if (row.m_id == ability->m_id)
                            slot = &row;
                if (!slot)
                {
                    AbilityInstance added;
                    added.m_serial     = list.m_nextSerial++;
                    added.m_id         = ability->m_id;
                    added.m_endLeft    = ability->m_endedType == 1 ? ability->m_endedValue : Fixed::fromInt(-1);
                    added.m_applicator = inst ? inst->m_applicator : self;
                    list.m_instances.push_back(added);
                    slot = &list.m_instances.back();
                }
                if (std::find(ability->m_events.begin(), ability->m_events.end(), TriggerPoint::InitAbility) !=
                    ability->m_events.end())
                    triggerAbility(world, self, *slot, *ability);
            }
        }
        break;
    case BuffOpKind::SpawnEffect:
        for (const int32_t id : op.m_ids)
        {
            SourceContext source;
            source.m_skillId = inst ? inst->m_skillId : -1;
            effect::rules::spawnFromRole(world, self, id, source);
        }
        break;
    case BuffOpKind::EnterCrazy:
        if (world.ecs().has<SkillCastComponent>(self))
            world.ecs().get<SkillCastComponent>(self).m_crazy = true;
        break;
    case BuffOpKind::Interrupt:
        if (world.ecs().has<SkillCastComponent>(self))
            skill::rules::forceInterrupt(world, self);
        break;
    case BuffOpKind::SetCost:
        break;
    case BuffOpKind::Sound:
        if (op.m_i0 > 0)
        {
            CueEvent event;
            event.m_entity = self;
            event.m_kind   = CueKind::Sound;
            event.m_id     = op.m_i0;
            world.ecs().singleton<EventQueue<CueEvent>>().push(event);
        }
        break;
    case BuffOpKind::ExitSelf:
        destroySerial(world, self, serial);
        break;
    case BuffOpKind::Choice:
    {
        if (inst->m_choice < 0)
        {
            int32_t count = static_cast<int32_t>(op.m_ids2.size());
            inst->m_choice = count > 0 ? pickWeighted(world, op.m_table) % count : 0;
        }
        const auto* def = buffDef(inst->m_id);
        if (!def || inst->m_choice < 0 || static_cast<size_t>(inst->m_choice) >= op.m_ids2.size())
            break;
        int32_t begin = op.m_choiceBegin;
        for (int32_t i = 0; i < inst->m_choice; ++i)
            begin += op.m_ids2[static_cast<size_t>(i)];
        const int32_t length = op.m_ids2[static_cast<size_t>(inst->m_choice)];
        for (int32_t i = 0; i < length; ++i)
        {
            const size_t index = static_cast<size_t>(begin + i);
            if (index < def->m_choicePool.size())
                execOp(exec, def->m_choicePool[index]);
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
    const Fixed saved = exec.m_reg;
    exec.m_reg        = {};
    for (const auto& op : ops)
    {
        if (!exec.m_fromAbility && !findInstance(*exec.m_world, exec.m_self, exec.m_serial))
            break;
        execOp(exec, op);
    }
    exec.m_reg = saved;
}

// 特殊能力的一次触发：条件、概率、冷却，然后执行降级 op。
void triggerAbility(World& world, Entity entity, AbilityInstance& ability, const AbilityDef& def)
{
    if (def.m_status != LowerStatus::Lowered)
        return;
    if (ability.m_cdLeft > Fixed{})
        return;
    Exec exec;
    exec.m_world       = &world;
    exec.m_self        = entity;
    exec.m_fromAbility = true;
    for (const auto& pred : def.m_conditions)
        if (!evalPredicate(exec, pred))
            return;
    if (def.m_probability < Fixed::fromInt(100))
    {
        const Fixed draw = rng::draw(world, rng::DrawKind::AbilityProb);
        if (draw > def.m_probability)
            return;
    }
    ability.m_cdLeft = world.setup().m_pvp ? def.m_cdPvp : def.m_cd;
    runOps(exec, def.m_ops);
}

// 钩子：首次合闩时先跑写入 r0 的瞬时 op，再合常驻，最后跑其余瞬时；已合闩则按需重施常驻后再瞬时。
void onHook(World& world, Entity entity, uint32_t serial, int32_t hook)
{
    auto* inst = findInstance(world, entity, serial);
    const auto* def = inst ? buffDef(inst->m_id) : nullptr;
    if (!inst || !def)
        return;
    const bool was      = inst->m_latched;
    const bool latchNow = !was && (def->m_activation.m_latchOn & hook) != 0;
    Exec exec;
    exec.m_world  = &world;
    exec.m_self   = entity;
    exec.m_serial = serial;
    exec.m_hook   = hook;
    const std::vector<BuffOp>* list = nullptr;
    if (hook == static_cast<int32_t>(BuffHook::Enter))
        list = &def->m_onEnter;
    else if (hook == static_cast<int32_t>(BuffHook::Tick))
        list = &def->m_onTick;
    else if (hook == static_cast<int32_t>(BuffHook::Begin))
        list = &def->m_onBegin;
    else if (hook == static_cast<int32_t>(BuffHook::End))
        list = &def->m_onEnd;
    else if (hook == static_cast<int32_t>(BuffHook::Exit))
        list = &def->m_onExit;
    auto stillHere = [&]() {
        return findInstance(*exec.m_world, exec.m_self, exec.m_serial) != nullptr;
    };
    auto writesReg = [](BuffOpKind kind) {
        return kind == BuffOpKind::Remove || kind == BuffOpKind::Count;
    };
    if (latchNow)
    {
        inst->m_latched    = true;
        exec.m_justLatched = true;
        // Remove/Count 先写入 r0，常驻 Stat(Reg) 才能读到；其余瞬时仍在常驻之后。
        if (list)
            for (const auto& op : *list)
            {
                if (!stillHere())
                    return;
                if (writesReg(op.m_kind))
                    execOp(exec, op);
            }
        if (!stillHere())
            return;
        for (const auto& op : def->m_persistent)
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
                if (!writesReg(op.m_kind))
                    execOp(exec, op);
            }
    }
    else
    {
        if (was && (def->m_activation.m_latchOn & hook) != 0)
        {
            for (const auto& op : def->m_persistent)
            {
                if (op.m_reapply ||
                    (op.m_kind == BuffOpKind::Kids && (hook & static_cast<int32_t>(BuffHook::Tick)) != 0) ||
                    (op.m_kind == BuffOpKind::Stat && op.m_live))
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
    if ((def->m_activation.m_releaseOn & hook) != 0 && inst->m_latched)
    {
        inst->m_latched = false;
        revoke(world, entity, serial);
    }
}

// 立即删除。退出钩子里若再次删除，序号已经不在。
void destroySerial(World& world, Entity entity, uint32_t serial)
{
    auto* inst = findInstance(world, entity, serial);
    if (!inst || !world.ecs().has<BuffListComponent>(entity))
        return;
    if (inst->m_removing)
        return;
    inst->m_removing = true;
    const int32_t id = inst->m_id;
    onHook(world, entity, serial, static_cast<int32_t>(BuffHook::Exit));
    inst = findInstance(world, entity, serial);
    if (inst && inst->m_latched)
    {
        inst->m_latched = false;
        // 退出钩子没有撤销时，仍撤销属性与标记，但留下已经加上的子 Buff。
        revoke(world, entity, serial, false);
    }
    auto& list = world.ecs().get<BuffListComponent>(entity);
    std::erase_if(list.m_instances, [serial](const BuffInstance& row) { return row.m_serial == serial; });
    BuffEvent event;
    event.m_entity = entity;
    event.m_buffId = id;
    event.m_serial = serial;
    event.m_phase  = BuffPhase::Removed;
    world.ecs().singleton<EventQueue<BuffEvent>>().push(event);
    BuffContext ctx;
    ctx.m_target = entity;
    rules::fire(world, entity, TriggerPoint::RemoveBuff, ctx);
}
}  // namespace mugen::sim::buff::detail
