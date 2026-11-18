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
    auto* inst = findInstance(*exec.world, exec.self, exec.serial);
    if (!inst)
        return exec.self;
    if (subject == Subject::EventTarget && exec.world->ecs().alive(inst->eventTarget) &&
        exec.world->ecs().has<IdentityComponent>(inst->eventTarget))
        return inst->eventTarget;
    if (subject == Subject::Applicator && exec.world->ecs().alive(inst->applicator) &&
        exec.world->ecs().has<IdentityComponent>(inst->applicator))
        return inst->applicator;
    return exec.self;
}

// 上限。
Fixed resourceMax(World& world, Entity entity, ResourceId res)
{
    if (!world.ecs().has<HealthComponent>(entity))
        return {};
    const auto& health = world.ecs().get<HealthComponent>(entity);
    if (res == ResourceId::Mp)
        return health.maxMp;
    if (res == ResourceId::Ep)
        return health.maxEp;
    if (!world.ecs().has<StatsComponent>(entity))
        return health.maxHp;
    return world.ecs().get<StatsComponent>(entity).maxHp();
}

// 当前值。
Fixed resourceCur(World& world, Entity entity, ResourceId res)
{
    if (!world.ecs().has<HealthComponent>(entity))
        return {};
    const auto& health = world.ecs().get<HealthComponent>(entity);
    if (res == ResourceId::Mp)
        return health.mp;
    if (res == ResourceId::Ep)
        return health.ep;
    return health.hp;
}

// 不含 Buff 加成的生命上限。
Fixed baseMaxHp(World& world, Entity entity)
{
    if (!world.ecs().has<StatsComponent>(entity))
        return {};
    return world.ecs().get<StatsComponent>(entity).base[4];
}

// 池中属于这些 buffType 的实例数。
int32_t countTypes(World& world, Entity entity, const std::vector<int32_t>& types)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return 0;
    int32_t count = 0;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
    {
        if (inst.removing)
            continue;
        const auto* def = buffDef(inst.id);
        if (!def)
            continue;
        if (std::find(types.begin(), types.end(), def->admission.buffType) != types.end())
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
    for (const auto& skill : book.skills)
    {
        if (!skill.timing || skill.cd <= Fixed{})
            continue;
        if (skill.slot < 0 || static_cast<size_t>(skill.slot) >= book.slots.size())
            continue;
        const auto& name = book.slots[static_cast<size_t>(skill.slot)];
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
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
    {
        if (inst.removing)
            continue;
        const auto* def = buffDef(inst.id);
        if (!def || def->admission.ruleId != rule)
            continue;
        if (minStacks < 0 || inst.stacks >= minStacks)
            return true;
    }
    return false;
}

// 是否持有 Buff。
bool hasBuff(World& world, Entity entity, int32_t id, int32_t minStacks)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return false;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
    {
        if (inst.removing || inst.id != id)
            continue;
        if (minStacks < 0 || inst.stacks >= minStacks)
            return true;
    }
    return false;
}

// 标记是否生效。
bool flagOn(World& world, Entity entity, BuffFlag flag)
{
    if (flag == BuffFlag::Crazy || flag == BuffFlag::NotCrazy)
    {
        const bool crazy = world.ecs().has<SkillCastComponent>(entity) && world.ecs().get<SkillCastComponent>(entity).crazy;
        return flag == BuffFlag::Crazy ? crazy : !crazy;
    }
    if (flag == BuffFlag::NotSuperArmor)
        return !world.ecs().has<StatusFlagsComponent>(entity) ||
               !world.ecs().get<StatusFlagsComponent>(entity).active(mugen::ExtraState::SuperArmor);
    if (flag == BuffFlag::NoHeal)
        return world.ecs().has<StatusFlagsComponent>(entity) && world.ecs().get<StatusFlagsComponent>(entity).noHeal > 0;
    if (flag == BuffFlag::NoCrystalCost)
        return world.ecs().has<StatusFlagsComponent>(entity) && world.ecs().get<StatusFlagsComponent>(entity).noCrystal > 0;
    if (!world.ecs().has<StatusFlagsComponent>(entity))
        return false;
    return world.ecs().get<StatusFlagsComponent>(entity).active(static_cast<mugen::ExtraState>(flag));
}

// 当前施法技能。
const SkillRuntime* casting(World& world, Entity entity)
{
    if (!world.ecs().has<SkillCastComponent>(entity) || !world.ecs().has<SkillBookComponent>(entity))
        return nullptr;
    const auto index = world.ecs().get<SkillCastComponent>(entity).skill;
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    if (index < 0 || static_cast<size_t>(index) >= book.skills.size())
        return nullptr;
    return &book.skills[static_cast<size_t>(index)];
}

// 槽位名。
std::string slotName(World& world, Entity entity, int32_t slot)
{
    if (!world.ecs().has<SkillBookComponent>(entity))
        return {};
    const auto& slots = world.ecs().get<SkillBookComponent>(entity).slots;
    if (slot < 0 || static_cast<size_t>(slot) >= slots.size())
        return {};
    return slots[static_cast<size_t>(slot)];
}

// 技能谓词。
bool evalSkill(World& world, Entity entity, const Predicate& pred, const BuffInstance& inst)
{
    const auto* skill = casting(world, entity);
    const auto kind   = static_cast<SkillCheck>(pred.subKind);
    if (kind == SkillCheck::OnCooldown)
    {
        if (!world.ecs().has<SkillBookComponent>(entity))
            return false;
        for (const auto& row : world.ecs().get<SkillBookComponent>(entity).skills)
            if (row.configId == pred.id && row.timing && row.cd > Fixed{})
                return true;
        return false;
    }
    if (!skill)
        return kind == SkillCheck::JudgeTable && pred.id == -1;
    const auto name = slotName(world, entity, skill->slot);
    switch (kind)
    {
    case SkillCheck::CommandAngle:
        return true;
    case SkillCheck::OpeningStep:
        return skill->step == 1;
    case SkillCheck::ChainFirst:
        return skill->step == 1;
    case SkillCheck::ChainLast:
        return skill->finally || skill->step >= skill->stepCount;
    case SkillCheck::ChainNotLast:
        return !skill->finally && skill->step < skill->stepCount;
    case SkillCheck::SameSlotNext:
        return inst.boundSkill > 0 && skill->configId != inst.boundSkill && name == slotName(world, entity, 0);
    case SkillCheck::DifferentSlot:
        return inst.boundSkill <= 0 || skill->configId != inst.boundSkill;
    case SkillCheck::SlotList:
    {
        for (const int32_t id : pred.ids)
            if (name.size() == 1 && name[0] == static_cast<char>('A' + id))
                return true;
        return std::find(pred.ids.begin(), pred.ids.end(), skill->slot) != pred.ids.end();
    }
    case SkillCheck::JudgeTable:
        if (pred.id == -1)
            return true;
        if (pred.id == 1)
            return name == "F" || (name.size() > 1 && name[0] == '6');
        if (pred.id == 2)
            return name == "Y" || name == "E" || (name.size() > 1);
        if (pred.id == 3)
            return name == "A" || (name.size() > 1);
        return false;
    case SkillCheck::SkillId:
        return skill->configId == pred.id ||
               std::find(pred.ids.begin(), pred.ids.end(), skill->configId) != pred.ids.end();
    case SkillCheck::ActionId:
    {
        if (!world.ecs().has<ActionTimelineComponent>(entity))
            return false;
        const int32_t clip = world.ecs().get<ActionTimelineComponent>(entity).clipId;
        if (pred.id != 0 && clip == pred.id)
            return true;
        return std::find(pred.ids.begin(), pred.ids.end(), clip) != pred.ids.end();
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
    for (auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
        if (inst.serial == serial)
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
    auto& world = *exec.world;
    auto* inst  = findInstance(world, exec.self, exec.serial);
    const int32_t stacks = inst ? inst->stacks : 1;
    const Entity self    = exec.self;
    switch (expr.kind)
    {
    case ValueKind::None:
        return {};
    case ValueKind::Const:
        return expr.a;
    case ValueKind::PerStack:
        return expr.a * Fixed::fromInt(stacks);
    case ValueKind::MaxRatio:
    {
        const bool ratio = expr.res == ResourceId::Hp ? (expr.a >= Fixed{} && expr.a < Fixed::fromInt(1))
                                                        : expr.a <= Fixed::fromInt(1);
        return ratio ? expr.a * resourceMax(world, self, expr.res) : expr.a;
    }
    case ValueKind::HpAmount:
    {
        Fixed value = expr.a;
        if (expr.a >= Fixed{} && expr.a < Fixed::fromInt(1))
            value *= resourceMax(world, self, ResourceId::Hp);
        if (expr.a < Fixed{} && expr.b > Fixed{})
        {
            const Fixed floor = expr.b - resourceCur(world, self, ResourceId::Hp);
            if (value < floor)
                value = floor;
        }
        return value;
    }
    case ValueKind::BaseMaxHp:
        return baseMaxHp(world, self) * expr.a;
    case ValueKind::Lost:
    {
        const Fixed cap = resourceMax(world, self, expr.res);
        if (cap <= Fixed{})
            return {};
        return expr.a * (cap - resourceCur(world, self, expr.res)) / cap;
    }
    case ValueKind::Cur:
    {
        const Fixed cap = resourceMax(world, self, expr.res);
        if (cap <= Fixed{})
            return {};
        return expr.a * resourceCur(world, self, expr.res) / cap;
    }
    case ValueKind::CurMul:
        return resourceCur(world, self, expr.res) * expr.a;
    case ValueKind::HpMaxAdjust:
    {
        const Fixed hp = resourceCur(world, self, ResourceId::Hp);
        const Fixed den = Fixed::fromInt(1) + expr.a;
        if (den == Fixed{})
            return {};
        return hp / den - hp;
    }
    case ValueKind::SpeedBonus:
    {
        Fixed bonus{};
        if (expr.a != Fixed{} && world.ecs().has<StatsComponent>(self))
            bonus = (world.ecs().get<StatsComponent>(self).finalStat(static_cast<int32_t>(BuffStat::MoveScaleX)) -
                     Fixed::fromInt(1)) /
                    expr.a * expr.b;
        return std::clamp(bonus, Fixed{}, expr.c);
    }
    case ValueKind::TypeCount:
    case ValueKind::MaxTimes:
    {
        const Entity pool = expr.pool == 2 && inst && world.ecs().alive(inst->eventTarget) ? inst->eventTarget : self;
        const Fixed counted = expr.a * Fixed::fromInt(countTypes(world, pool, expr.types));
        Fixed value         = expr.b > Fixed{} ? std::min(counted, expr.b) : counted;
        if (expr.kind == ValueKind::MaxTimes)
            value = resourceMax(world, self, expr.res == ResourceId::None ? ResourceId::Hp : expr.res) * value;
        return value;
    }
    case ValueKind::CoolingSlots:
    {
        const Fixed counted = expr.a * Fixed::fromInt(coolingBaseSlots(world, self));
        return expr.b > Fixed{} ? std::min(counted, expr.b) : counted;
    }
    case ValueKind::CoolingSlotTable:
    {
        const auto index = coolingBaseSlots(world, self);
        if (index < 0 || static_cast<size_t>(index) >= expr.table.size())
            return {};
        return expr.table[static_cast<size_t>(index)];
    }
    case ValueKind::ComboTable:
    {
        int32_t grade = 0;
        if (world.ecs().has<ComboMeterComponent>(self))
            grade = world.ecs().get<ComboMeterComponent>(self).grade;
        if (grade < 0 || static_cast<size_t>(grade) >= expr.table.size())
            return expr.bias;
        return expr.table[static_cast<size_t>(grade)] + expr.bias;
    }
    case ValueKind::LastDamage:
        return (inst ? inst->damage : Fixed{}) * expr.a;
    case ValueKind::Reg:
        return exec.reg * expr.a;
    case ValueKind::RegOfMax:
        return resourceMax(world, self, expr.res) * exec.reg * expr.a;
    }
    return {};
}

// 条件谓词。
bool evalPredicate(Exec& exec, const Predicate& pred)
{
    if (pred.kind == PredKind::None)
        return true;
    if (pred.effect != OpEffect::Active)
        return false;
    auto& world      = *exec.world;
    const Entity who = subjectOf(exec, pred.subject);
    auto* inst       = findInstance(world, exec.self, exec.serial);
    switch (pred.kind)
    {
    case PredKind::HasRule:
    {
        if (pred.allOf)
        {
            for (const int32_t id : pred.ids)
                if (!hasRule(world, who, id, pred.minStacks))
                    return false;
            return !pred.ids.empty();
        }
        if (!pred.ids.empty())
        {
            for (const int32_t id : pred.ids)
                if (hasRule(world, who, id, pred.minStacks))
                    return true;
            return false;
        }
        return hasRule(world, who, pred.id, pred.minStacks);
    }
    case PredKind::HasBuff:
        return hasBuff(world, who, pred.id, pred.minStacks);
    case PredKind::HasBuffType:
    {
        if (!world.ecs().has<BuffListComponent>(who))
            return false;
        for (const auto& row : world.ecs().get<BuffListComponent>(who).instances)
        {
            const auto* def = buffDef(row.id);
            if (row.removing || !def)
                continue;
            if (def->admission.buffType == pred.id)
                return true;
            if (std::find(pred.ids.begin(), pred.ids.end(), def->admission.buffType) != pred.ids.end())
                return true;
        }
        return false;
    }
    case PredKind::HasFlag:
    {
        if (!pred.ids.empty())
        {
            for (const int32_t id : pred.ids)
                if (flagOn(world, who, static_cast<BuffFlag>(id)))
                    return true;
            return false;
        }
        return flagOn(world, who, static_cast<BuffFlag>(pred.id));
    }
    case PredKind::Motion:
    {
        const auto scale = world.ecs().has<StatsComponent>(who)
                               ? world.ecs().get<StatsComponent>(who).finalStat(static_cast<int32_t>(BuffStat::MoveScaleX))
                               : Fixed::fromInt(1);
        const bool air = world.ecs().has<TransformComponent>(who) && world.ecs().get<TransformComponent>(who).position.y > Fixed{};
        switch (static_cast<MotionCheck>(pred.subKind))
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
        const ResourceId which = pred.subKind == static_cast<int32_t>(RatioRes::Mp)   ? ResourceId::Mp
                                 : pred.subKind == static_cast<int32_t>(RatioRes::Ep) ? ResourceId::Ep
                                                                                        : ResourceId::Hp;
        const Fixed cap = resourceMax(world, who, which);
        if (cap <= Fixed{})
            return false;
        const Fixed ratio = resourceCur(world, who, which) / cap;
        if (pred.hasMin)
        {
            if (pred.minInclusive ? ratio < pred.min : ratio <= pred.min)
                return false;
        }
        if (pred.hasMax)
        {
            if (pred.maxInclusive ? ratio > pred.max : ratio >= pred.max)
                return false;
        }
        return true;
    }
    case PredKind::Delta:
    {
        if (!world.ecs().has<HealthComponent>(who))
            return false;
        const auto& health = world.ecs().get<HealthComponent>(who);
        switch (static_cast<DeltaCheck>(pred.subKind))
        {
        case DeltaCheck::LostHp:
            return health.hp < health.prevHp;
        case DeltaCheck::RecoveredHp:
            return health.hp > health.prevHp;
        case DeltaCheck::LostEp:
            return health.ep < health.prevEp;
        case DeltaCheck::RecoveredEp:
            return health.ep > health.prevEp;
        case DeltaCheck::HpChangePercent:
        {
            if (health.prevHp <= Fixed{})
                return false;
            const Fixed delta = (health.hp - health.prevHp) / health.prevHp;
            return delta >= pred.min && (pred.max == Fixed{} || delta <= pred.max);
        }
        default:
            return false;
        }
    }
    case PredKind::RoleType:
    {
        if (!world.ecs().has<IdentityComponent>(who))
            return false;
        const auto type = world.ecs().get<IdentityComponent>(who).roleType;
        switch (static_cast<RoleCheck>(pred.subKind))
        {
        case RoleCheck::Monster:
            return type == 4;
        case RoleCheck::Hero:
            return type == 2 || type == 3 || type == 1;
        case RoleCheck::TypeEq:
            return type == pred.id || std::find(pred.ids.begin(), pred.ids.end(), type) != pred.ids.end();
        case RoleCheck::Sign:
            return std::find(pred.ids.begin(), pred.ids.end(), type) != pred.ids.end();
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
        switch (static_cast<EventCheck>(pred.subKind))
        {
        case EventCheck::TargetDead:
            return inst->targetDead ||
                   (world.ecs().alive(inst->eventTarget) && world.ecs().has<HealthComponent>(inst->eventTarget) &&
                    world.ecs().get<HealthComponent>(inst->eventTarget).dead);
        case EventCheck::Crit:
        case EventCheck::AbilityCrit:
            return inst->critical;
        case EventCheck::DarkHit:
        case EventCheck::LightHit:
            return inst->effectId != 0;
        case EventCheck::UseTp:
            return inst->tpSpent > Fixed{};
        default:
            return false;
        }
    }
    case PredKind::Timer:
    {
        if (!world.ecs().has<RoleStateComponent>(who))
            return false;
        const auto& state = world.ecs().get<RoleStateComponent>(who);
        switch (static_cast<TimerCheck>(pred.subKind))
        {
        case TimerCheck::Moved:
            return state.movedSec >= pred.min;
        case TimerCheck::Standing:
            return state.standingSec >= pred.min;
        case TimerCheck::Moving:
            // 与冻结实现的 isMoved 一致：连续移动秒数大于阈值才算移动过。
            return state.movedSec > pred.min;
        default:
            return false;
        }
    }
    case PredKind::Misc:
    {
        switch (static_cast<MiscCheck>(pred.subKind))
        {
        case MiscCheck::MpEnough:
            return world.ecs().has<HealthComponent>(who) && world.ecs().get<HealthComponent>(who).mp >= pred.min;
        case MiscCheck::CrystalEnough:
            return false;
        case MiscCheck::ComboEval:
        {
            const Fixed need = pred.hasMin ? pred.min : Fixed::fromInt(pred.id);
            return world.ecs().has<ComboMeterComponent>(who) &&
                   Fixed::fromInt(world.ecs().get<ComboMeterComponent>(who).grade) >= need;
        }
        case MiscCheck::RageTransform:
            return world.ecs().has<RageStateComponent>(who) && world.ecs().get<RageStateComponent>(who).transformed;
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
