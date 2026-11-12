#include "mugen/sim/buff/BuffInternal.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim::buff::detail
{
namespace
{
using namespace conf::battle;

// 主体。事件目标或施加者无效时退回自身。
Entity subjectOf(const Exec& exec, Subject subject)
{
    auto* inst = findInstance(*exec.m_world, exec.m_self, exec.m_serial);
    if (!inst)
        return exec.m_self;
    if (subject == Subject::EventTarget && exec.m_world->ecs().alive(inst->m_eventTarget) &&
        exec.m_world->ecs().has<IdentityComponent>(inst->m_eventTarget))
        return inst->m_eventTarget;
    if (subject == Subject::Applicator && exec.m_world->ecs().alive(inst->m_applicator) &&
        exec.m_world->ecs().has<IdentityComponent>(inst->m_applicator))
        return inst->m_applicator;
    return exec.m_self;
}

// 上限。
Fixed resourceMax(World& world, Entity entity, ResourceId res)
{
    if (!world.ecs().has<HealthComponent>(entity))
        return {};
    const auto& health = world.ecs().get<HealthComponent>(entity);
    if (res == ResourceId::Mp)
        return health.m_maxMp;
    if (res == ResourceId::Ep)
        return health.m_maxEp;
    if (!world.ecs().has<StatsComponent>(entity))
        return health.m_maxHp;
    return world.ecs().get<StatsComponent>(entity).maxHp();
}

// 当前值。
Fixed resourceCur(World& world, Entity entity, ResourceId res)
{
    if (!world.ecs().has<HealthComponent>(entity))
        return {};
    const auto& health = world.ecs().get<HealthComponent>(entity);
    if (res == ResourceId::Mp)
        return health.m_mp;
    if (res == ResourceId::Ep)
        return health.m_ep;
    return health.m_hp;
}

// 不含 Buff 加成的生命上限。
Fixed baseMaxHp(World& world, Entity entity)
{
    if (!world.ecs().has<StatsComponent>(entity))
        return {};
    return world.ecs().get<StatsComponent>(entity).m_base[4];
}

// 池中属于这些 buffType 的实例数。
int32_t countTypes(World& world, Entity entity, const std::vector<int32_t>& types)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return 0;
    int32_t count = 0;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).m_instances)
    {
        if (inst.m_removing)
            continue;
        const auto* def = buffDef(inst.m_id);
        if (!def)
            continue;
        if (std::find(types.begin(), types.end(), def->m_admission.m_buffType) != types.end())
            ++count;
    }
    return count;
}

// 冷却中的基础槽数。
int32_t coolingBaseSlots(World& world, Entity entity)
{
    if (!world.ecs().has<SkillBookComponent>(entity))
        return 0;
    int32_t count = 0;
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    for (const auto& skill : book.m_skills)
    {
        if (!skill.m_timing || skill.m_cd <= Fixed{})
            continue;
        if (skill.m_slot < 0 || static_cast<size_t>(skill.m_slot) >= book.m_slots.size())
            continue;
        const auto& name = book.m_slots[static_cast<size_t>(skill.m_slot)];
        if (name == "A" || name == "B" || name == "C" || name == "D" || name == "E" || name == "F" || name == "Y")
            ++count;
    }
    return count;
}

// 是否持有规则。
bool hasRule(World& world, Entity entity, int32_t rule, int32_t minStacks)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return false;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).m_instances)
    {
        if (inst.m_removing)
            continue;
        const auto* def = buffDef(inst.m_id);
        if (!def || def->m_admission.m_ruleId != rule)
            continue;
        if (minStacks < 0 || inst.m_stacks >= minStacks)
            return true;
    }
    return false;
}

// 是否持有 Buff。
bool hasBuff(World& world, Entity entity, int32_t id, int32_t minStacks)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return false;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).m_instances)
    {
        if (inst.m_removing || inst.m_id != id)
            continue;
        if (minStacks < 0 || inst.m_stacks >= minStacks)
            return true;
    }
    return false;
}

// 标记是否生效。
bool flagOn(World& world, Entity entity, BuffFlag flag)
{
    if (flag == BuffFlag::Crazy || flag == BuffFlag::NotCrazy)
    {
        const bool crazy = world.ecs().has<SkillCastComponent>(entity) && world.ecs().get<SkillCastComponent>(entity).m_crazy;
        return flag == BuffFlag::Crazy ? crazy : !crazy;
    }
    if (flag == BuffFlag::NotSuperArmor)
        return !world.ecs().has<StatusFlagsComponent>(entity) ||
               !world.ecs().get<StatusFlagsComponent>(entity).active(mugen::ExtraState::SuperArmor);
    if (flag == BuffFlag::NoHeal)
        return world.ecs().has<StatusFlagsComponent>(entity) && world.ecs().get<StatusFlagsComponent>(entity).m_noHeal > 0;
    if (flag == BuffFlag::NoCrystalCost)
        return world.ecs().has<StatusFlagsComponent>(entity) && world.ecs().get<StatusFlagsComponent>(entity).m_noCrystal > 0;
    if (!world.ecs().has<StatusFlagsComponent>(entity))
        return false;
    return world.ecs().get<StatusFlagsComponent>(entity).active(static_cast<mugen::ExtraState>(flag));
}

// 当前施法技能。
const SkillRuntime* casting(World& world, Entity entity)
{
    if (!world.ecs().has<SkillCastComponent>(entity) || !world.ecs().has<SkillBookComponent>(entity))
        return nullptr;
    const auto index = world.ecs().get<SkillCastComponent>(entity).m_skill;
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    if (index < 0 || static_cast<size_t>(index) >= book.m_skills.size())
        return nullptr;
    return &book.m_skills[static_cast<size_t>(index)];
}

// 槽位名。
std::string slotName(World& world, Entity entity, int32_t slot)
{
    if (!world.ecs().has<SkillBookComponent>(entity))
        return {};
    const auto& slots = world.ecs().get<SkillBookComponent>(entity).m_slots;
    if (slot < 0 || static_cast<size_t>(slot) >= slots.size())
        return {};
    return slots[static_cast<size_t>(slot)];
}

// 技能谓词。
bool evalSkill(World& world, Entity entity, const Predicate& pred, const BuffInstance& inst)
{
    const auto* skill = casting(world, entity);
    const auto kind   = static_cast<SkillCheck>(pred.m_subKind);
    if (kind == SkillCheck::OnCooldown)
    {
        if (!world.ecs().has<SkillBookComponent>(entity))
            return false;
        for (const auto& row : world.ecs().get<SkillBookComponent>(entity).m_skills)
            if (row.m_configId == pred.m_id && row.m_timing && row.m_cd > Fixed{})
                return true;
        return false;
    }
    if (!skill)
        return kind == SkillCheck::JudgeTable && pred.m_id == -1;
    const auto name = slotName(world, entity, skill->m_slot);
    switch (kind)
    {
    case SkillCheck::CommandAngle:
        return true;
    case SkillCheck::OpeningStep:
        return skill->m_step == 1;
    case SkillCheck::ChainFirst:
        return skill->m_step == 1;
    case SkillCheck::ChainLast:
        return skill->m_finally || skill->m_step >= skill->m_stepCount;
    case SkillCheck::ChainNotLast:
        return !skill->m_finally && skill->m_step < skill->m_stepCount;
    case SkillCheck::SameSlotNext:
        return inst.m_boundSkill > 0 && skill->m_configId != inst.m_boundSkill && name == slotName(world, entity, 0);
    case SkillCheck::DifferentSlot:
        return inst.m_boundSkill <= 0 || skill->m_configId != inst.m_boundSkill;
    case SkillCheck::SlotList:
    {
        for (const int32_t id : pred.m_ids)
            if (name.size() == 1 && name[0] == static_cast<char>('A' + id))
                return true;
        return std::find(pred.m_ids.begin(), pred.m_ids.end(), skill->m_slot) != pred.m_ids.end();
    }
    case SkillCheck::JudgeTable:
        if (pred.m_id == -1)
            return true;
        if (pred.m_id == 1)
            return name == "F" || (name.size() > 1 && name[0] == '6');
        if (pred.m_id == 2)
            return name == "Y" || name == "E" || (name.size() > 1);
        if (pred.m_id == 3)
            return name == "A" || (name.size() > 1);
        return false;
    case SkillCheck::SkillId:
        return skill->m_configId == pred.m_id ||
               std::find(pred.m_ids.begin(), pred.m_ids.end(), skill->m_configId) != pred.m_ids.end();
    case SkillCheck::ActionId:
    {
        if (!world.ecs().has<ActionTimelineComponent>(entity))
            return false;
        const int32_t clip = world.ecs().get<ActionTimelineComponent>(entity).m_clipId;
        if (pred.m_id != 0 && clip == pred.m_id)
            return true;
        return std::find(pred.m_ids.begin(), pred.m_ids.end(), clip) != pred.m_ids.end();
    }
    default:
        return false;
    }
}
}  // namespace

// 按序号查找。
BuffInstance* findInstance(World& world, Entity entity, uint32_t serial)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return nullptr;
    for (auto& inst : world.ecs().get<BuffListComponent>(entity).m_instances)
        if (inst.m_serial == serial)
            return &inst;
    return nullptr;
}

// 配置查找。
const BuffDef* buffDef(int32_t id)
{
    return conf::Config::instance().find<BuffDef>(id);
}

// 取值表达式。
Fixed evalValue(Exec& exec, const ValueExpr& expr)
{
    auto& world = *exec.m_world;
    auto* inst  = findInstance(world, exec.m_self, exec.m_serial);
    const int32_t stacks = inst ? inst->m_stacks : 1;
    const Entity self    = exec.m_self;
    switch (expr.m_kind)
    {
    case ValueKind::None:
        return {};
    case ValueKind::Const:
        return expr.m_a;
    case ValueKind::PerStack:
        return expr.m_a * Fixed::fromInt(stacks);
    case ValueKind::MaxRatio:
    {
        const bool ratio = expr.m_res == ResourceId::Hp ? (expr.m_a >= Fixed{} && expr.m_a < Fixed::fromInt(1))
                                                        : expr.m_a <= Fixed::fromInt(1);
        return ratio ? expr.m_a * resourceMax(world, self, expr.m_res) : expr.m_a;
    }
    case ValueKind::HpAmount:
    {
        Fixed value = expr.m_a;
        if (expr.m_a >= Fixed{} && expr.m_a < Fixed::fromInt(1))
            value *= resourceMax(world, self, ResourceId::Hp);
        if (expr.m_a < Fixed{} && expr.m_b > Fixed{})
        {
            const Fixed floor = expr.m_b - resourceCur(world, self, ResourceId::Hp);
            if (value < floor)
                value = floor;
        }
        return value;
    }
    case ValueKind::BaseMaxHp:
        return baseMaxHp(world, self) * expr.m_a;
    case ValueKind::Lost:
    {
        const Fixed cap = resourceMax(world, self, expr.m_res);
        if (cap <= Fixed{})
            return {};
        return expr.m_a * (cap - resourceCur(world, self, expr.m_res)) / cap;
    }
    case ValueKind::Cur:
    {
        const Fixed cap = resourceMax(world, self, expr.m_res);
        if (cap <= Fixed{})
            return {};
        return expr.m_a * resourceCur(world, self, expr.m_res) / cap;
    }
    case ValueKind::CurMul:
        return resourceCur(world, self, expr.m_res) * expr.m_a;
    case ValueKind::HpMaxAdjust:
    {
        const Fixed hp = resourceCur(world, self, ResourceId::Hp);
        const Fixed den = Fixed::fromInt(1) + expr.m_a;
        if (den == Fixed{})
            return {};
        return hp / den - hp;
    }
    case ValueKind::SpeedBonus:
    {
        Fixed bonus{};
        if (expr.m_a != Fixed{} && world.ecs().has<StatsComponent>(self))
            bonus = (world.ecs().get<StatsComponent>(self).finalStat(static_cast<int32_t>(BuffStat::MoveScaleX)) -
                     Fixed::fromInt(1)) /
                    expr.m_a * expr.m_b;
        return std::clamp(bonus, Fixed{}, expr.m_c);
    }
    case ValueKind::TypeCount:
    case ValueKind::MaxTimes:
    {
        const Entity pool = expr.m_pool == 2 && inst && world.ecs().alive(inst->m_eventTarget) ? inst->m_eventTarget : self;
        const Fixed counted = expr.m_a * Fixed::fromInt(countTypes(world, pool, expr.m_types));
        Fixed value         = expr.m_b > Fixed{} ? std::min(counted, expr.m_b) : counted;
        if (expr.m_kind == ValueKind::MaxTimes)
            value = resourceMax(world, self, expr.m_res == ResourceId::None ? ResourceId::Hp : expr.m_res) * value;
        return value;
    }
    case ValueKind::CoolingSlots:
    {
        const Fixed counted = expr.m_a * Fixed::fromInt(coolingBaseSlots(world, self));
        return expr.m_b > Fixed{} ? std::min(counted, expr.m_b) : counted;
    }
    case ValueKind::CoolingSlotTable:
    {
        const auto index = coolingBaseSlots(world, self);
        if (index < 0 || static_cast<size_t>(index) >= expr.m_table.size())
            return {};
        return expr.m_table[static_cast<size_t>(index)];
    }
    case ValueKind::ComboTable:
    {
        int32_t grade = 0;
        if (world.ecs().has<ComboMeterComponent>(self))
            grade = world.ecs().get<ComboMeterComponent>(self).m_grade;
        if (grade < 0 || static_cast<size_t>(grade) >= expr.m_table.size())
            return expr.m_bias;
        return expr.m_table[static_cast<size_t>(grade)] + expr.m_bias;
    }
    case ValueKind::LastDamage:
        return (inst ? inst->m_damage : Fixed{}) * expr.m_a;
    case ValueKind::Reg:
        return exec.m_reg * expr.m_a;
    case ValueKind::RegOfMax:
        return resourceMax(world, self, expr.m_res) * exec.m_reg * expr.m_a;
    }
    return {};
}

// 条件谓词。
bool evalPredicate(Exec& exec, const Predicate& pred)
{
    if (pred.m_kind == PredKind::None)
        return true;
    if (pred.m_effect != OpEffect::Active)
        return false;
    auto& world      = *exec.m_world;
    const Entity who = subjectOf(exec, pred.m_subject);
    auto* inst       = findInstance(world, exec.m_self, exec.m_serial);
    switch (pred.m_kind)
    {
    case PredKind::HasRule:
    {
        if (pred.m_allOf)
        {
            for (const int32_t id : pred.m_ids)
                if (!hasRule(world, who, id, pred.m_minStacks))
                    return false;
            return !pred.m_ids.empty();
        }
        if (!pred.m_ids.empty())
        {
            for (const int32_t id : pred.m_ids)
                if (hasRule(world, who, id, pred.m_minStacks))
                    return true;
            return false;
        }
        return hasRule(world, who, pred.m_id, pred.m_minStacks);
    }
    case PredKind::HasBuff:
        return hasBuff(world, who, pred.m_id, pred.m_minStacks);
    case PredKind::HasBuffType:
    {
        if (!world.ecs().has<BuffListComponent>(who))
            return false;
        for (const auto& row : world.ecs().get<BuffListComponent>(who).m_instances)
        {
            const auto* def = buffDef(row.m_id);
            if (row.m_removing || !def)
                continue;
            if (def->m_admission.m_buffType == pred.m_id)
                return true;
            if (std::find(pred.m_ids.begin(), pred.m_ids.end(), def->m_admission.m_buffType) != pred.m_ids.end())
                return true;
        }
        return false;
    }
    case PredKind::HasFlag:
    {
        if (!pred.m_ids.empty())
        {
            for (const int32_t id : pred.m_ids)
                if (flagOn(world, who, static_cast<BuffFlag>(id)))
                    return true;
            return false;
        }
        return flagOn(world, who, static_cast<BuffFlag>(pred.m_id));
    }
    case PredKind::Motion:
    {
        const auto scale = world.ecs().has<StatsComponent>(who)
                               ? world.ecs().get<StatsComponent>(who).finalStat(static_cast<int32_t>(BuffStat::MoveScaleX))
                               : Fixed::fromInt(1);
        const bool air = world.ecs().has<TransformComponent>(who) && world.ecs().get<TransformComponent>(who).m_position.y > Fixed{};
        switch (static_cast<MotionCheck>(pred.m_subKind))
        {
        case MotionCheck::SpeedUp:
            return scale > Fixed::fromInt(1);
        case MotionCheck::SpeedDown:
            return scale < Fixed::fromInt(1);
        case MotionCheck::Air:
            return air;
        case MotionCheck::NotAir:
            return !air;
        default:
            return false;
        }
    }
    case PredKind::Ratio:
    {
        const ResourceId which = pred.m_subKind == static_cast<int32_t>(RatioRes::Mp)   ? ResourceId::Mp
                                 : pred.m_subKind == static_cast<int32_t>(RatioRes::Ep) ? ResourceId::Ep
                                                                                        : ResourceId::Hp;
        const Fixed cap = resourceMax(world, who, which);
        if (cap <= Fixed{})
            return false;
        const Fixed ratio = resourceCur(world, who, which) / cap;
        if (pred.m_hasMin)
        {
            if (pred.m_minInclusive ? ratio < pred.m_min : ratio <= pred.m_min)
                return false;
        }
        if (pred.m_hasMax)
        {
            if (pred.m_maxInclusive ? ratio > pred.m_max : ratio >= pred.m_max)
                return false;
        }
        return true;
    }
    case PredKind::Delta:
    {
        if (!world.ecs().has<HealthComponent>(who))
            return false;
        const auto& health = world.ecs().get<HealthComponent>(who);
        switch (static_cast<DeltaCheck>(pred.m_subKind))
        {
        case DeltaCheck::LostHp:
            return health.m_hp < health.m_prevHp;
        case DeltaCheck::RecoveredHp:
            return health.m_hp > health.m_prevHp;
        case DeltaCheck::LostEp:
            return health.m_ep < health.m_prevEp;
        case DeltaCheck::RecoveredEp:
            return health.m_ep > health.m_prevEp;
        case DeltaCheck::HpChangePercent:
        {
            if (health.m_prevHp <= Fixed{})
                return false;
            const Fixed delta = (health.m_hp - health.m_prevHp) / health.m_prevHp;
            return delta >= pred.m_min && (pred.m_max == Fixed{} || delta <= pred.m_max);
        }
        default:
            return false;
        }
    }
    case PredKind::RoleType:
    {
        if (!world.ecs().has<IdentityComponent>(who))
            return false;
        const auto type = world.ecs().get<IdentityComponent>(who).m_roleType;
        switch (static_cast<RoleCheck>(pred.m_subKind))
        {
        case RoleCheck::Monster:
            return type == 4;
        case RoleCheck::Hero:
            return type == 2 || type == 3 || type == 1;
        case RoleCheck::TypeEq:
            return type == pred.m_id || std::find(pred.m_ids.begin(), pred.m_ids.end(), type) != pred.m_ids.end();
        case RoleCheck::Sign:
            return std::find(pred.m_ids.begin(), pred.m_ids.end(), type) != pred.m_ids.end();
        default:
            return false;
        }
    }
    case PredKind::Skill:
        return inst && evalSkill(world, who, pred, *inst);
    case PredKind::Artifact:
        return false;
    case PredKind::Event:
    {
        if (!inst)
            return false;
        switch (static_cast<EventCheck>(pred.m_subKind))
        {
        case EventCheck::TargetDead:
            return inst->m_targetDead ||
                   (world.ecs().alive(inst->m_eventTarget) && world.ecs().has<HealthComponent>(inst->m_eventTarget) &&
                    world.ecs().get<HealthComponent>(inst->m_eventTarget).m_dead);
        case EventCheck::Crit:
        case EventCheck::AbilityCrit:
            return inst->m_critical;
        case EventCheck::DarkHit:
        case EventCheck::LightHit:
            return inst->m_effectId != 0;
        case EventCheck::UseTp:
            return inst->m_tpSpent > Fixed{};
        default:
            return false;
        }
    }
    case PredKind::Timer:
    {
        if (!world.ecs().has<RoleStateComponent>(who))
            return false;
        const auto& state = world.ecs().get<RoleStateComponent>(who);
        switch (static_cast<TimerCheck>(pred.m_subKind))
        {
        case TimerCheck::Moved:
            return state.m_movedSec >= pred.m_min;
        case TimerCheck::Standing:
            return state.m_standingSec >= pred.m_min;
        case TimerCheck::Moving:
            // 与冻结实现的 isMoved 一致：连续移动秒数大于阈值才算移动过。
            return state.m_movedSec > pred.m_min;
        default:
            return false;
        }
    }
    case PredKind::Misc:
    {
        switch (static_cast<MiscCheck>(pred.m_subKind))
        {
        case MiscCheck::MpEnough:
            return world.ecs().has<HealthComponent>(who) && world.ecs().get<HealthComponent>(who).m_mp >= pred.m_min;
        case MiscCheck::CrystalEnough:
            return false;
        case MiscCheck::ComboEval:
        {
            const Fixed need = pred.m_hasMin ? pred.m_min : Fixed::fromInt(pred.m_id);
            return world.ecs().has<ComboMeterComponent>(who) &&
                   Fixed::fromInt(world.ecs().get<ComboMeterComponent>(who).m_grade) >= need;
        }
        case MiscCheck::RageTransform:
            return world.ecs().has<RageStateComponent>(who) && world.ecs().get<RageStateComponent>(who).m_transformed;
        case MiscCheck::Never:
            return false;
        default:
            return false;
        }
    }
    default:
        return false;
    }
}
}  // namespace mugen::sim::buff::detail
