#include "BuffHolder.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
namespace
{
// 条件参数缺省值为零，不使用临时浮点转换。
Fixed parameter(const std::vector<Fixed>& values, size_t index = 0)
{
    return index < values.size() ? values[index] : Fixed{};
}
// 查询第一个对应规则实例，保持池插入顺序。
const BuffInstance* byRule(World& world, Entity entity, int32_t ruleId)
{
    const auto* pool = world.ecs().tryGet<BuffPool>(entity);
    if (!pool) return nullptr;
    for (const auto& value : pool->m_instances)
    {
        const auto* config = conf::Config::instance().find<conf::BuffConfig>(value.m_id);
        if (config && config->ruleId == ruleId) return &value;
    }
    return nullptr;
}
// 从当前技能配置标识解析所属槽内实例。
const SkillBase* currentSkill(World& world, Entity entity, const BuffInstance& instance)
{
    const auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (!pool) return nullptr;
    if (instance.m_context.m_currentSkill)
    {
        for (const auto& skill : pool->m_skills)
            if (skill.m_configId == *instance.m_context.m_currentSkill) return &skill;
        return nullptr;
    }
    return pool->m_current >= 0 ? &pool->m_skills.at(pool->m_current) : nullptr;
}
// 正阈值表示至少达到，非正阈值表示至多达到其绝对值。
bool ratioMatches(Fixed current, Fixed maximum, Fixed threshold)
{
    if (maximum == Fixed{}) return false;
    const auto ratio = current / maximum;
    return threshold > Fixed{} ? ratio >= threshold : ratio <= -threshold;
}
// 条件编号与规则类名映射保留原配置编号，不依赖规则主键排序。
struct NamedCondition
{
    // 目标条件编号、自身条件编号和规则类名。
    int32_t m_target, m_entity;
    std::string_view m_name;
};
// 只读规则存在性条件列表。
constexpr NamedCondition kNamedConditions[] = {
    {108,109,"BuffSneer"},{110,111,"BuffBurn"},{118,119,"BuffSign"},{120,121,"BuffBleeding"},
    {128,129,"BuffPoison"},{130,131,"BuffDark"},{132,133,"BuffThunder1"},{134,135,"BuffThunder2"},
    {136,137,"BuffThunder3"},{138,139,"BuffPlague1"},{140,141,"BuffPlague2"},{142,143,"BuffPlague3"},
    {146,147,"BuffBreakArmor"},{148,149,"BuffMagicUp"},{152,153,"BuffDark2"},{156,179,"BuffDeathMark"},
    {158,157,"BuffCold"},{160,159,"BuffPassivation"},{170,178,"BuffPawMark"},{176,177,"BuffBlind"},
    {180,181,"BuffRepent"},{182,183,"BuffFall"},{184,185,"BuffPurplePoison"},{186,187,"BuffDurance"},
    {188,189,"BuffPalsy"},{190,191,"BuffBurn2"},{192,193,"BuffChill"},{194,195,"BuffFlaw"},
    {196,197,"BuffStarShield"},{199,200,"BuffGhostWalk"},{201,202,"BuffGuard"},{203,204,"BuffBloodBeast"},
    {205,206,"BuffFlames"},{214,207,"BuffRedPython"},{208,209,"BuffMeltingArmor"},{211,210,"BuffKillIntent"},
    {212,213,"BuffSeparate"},{218,219,"BuffLightMark"},{220,221,"BuffStoneSkin"},{222,223,"BuffAdjudication"},
    {224,225,"BuffReverse"},{226,227,"BuffElectricEye"},{228,229,"BuffLightWing"},{230,231,"BuffSpeedStep"},
    {234,235,"BuffMercenary"},{236,237,"BuffUnstable"},{244,245,"BuffDarkMark"},{250,251,"BuffMight"},
    {252,253,"BuffElectron"},{254,255,"BuffElectron2"},{261,262,"BuffDarkMark2"}
};
// 单项条件访问当前实体数据，未支持条件明确报错而非默认为通过。
bool checkOne(World& world, Entity entity, const BuffInstance& instance, int32_t condition,
              const std::vector<Fixed>& params)
{
    const auto target = instance.m_context.m_target;
    for (const auto& mapping : kNamedConditions)
    {
        if (condition != mapping.m_target && condition != mapping.m_entity) continue;
        const auto subject = condition == mapping.m_target ? target : entity;
        for (const auto& rule : conf::Config::instance().table<conf::BuffRuleConfig>())
            if (rule.className == mapping.m_name)
            {
                const auto* found = byRule(world, subject, rule.id);
                return found && (parameter(params) == "-1"_fx ||
                    Fixed::fromInt(found->m_lifecycle.m_repeat) >= parameter(params));
            }
        return false;
    }
    switch (condition)
    {
    case 0: return true;
    case 1: return world.ecs().has<Vitals>(target) && Role(world, target).isDead();
    case 2: {
        const auto* state = world.ecs().tryGet<RoleHitState>(entity);
        return state && state->m_critical;
    }
    case 3: case 4: case 173: {
        const auto* role = world.ecs().tryGet<RoleIdentity>(target);
        if (!role) return false;
        return condition == 3 ? role->roleType == 4 : condition == 4 ?
            (role->roleType == 2 || role->roleType == 3) : role->roleType == parameter(params).toInt();
    }
    case 5: {
        const auto* skill = currentSkill(world, entity, instance);
        return skill && skill->m_controlAngle.has_value();
    }
    case 7: {
        const auto* pool = world.ecs().tryGet<SkillPool>(entity);
        return pool && Fixed::fromInt(pool->m_crystals) >= parameter(params);
    }
    case 6: {
        const auto* pool = world.ecs().tryGet<SkillPool>(entity);
        const auto* vitals = world.ecs().tryGet<Vitals>(entity);
        if (!pool || !vitals) return false;
        const SkillBase* selected = currentSkill(world, entity, instance);
        if (instance.m_context.m_boundSkill)
        {
            selected = nullptr;
            for (const auto& value : pool->m_skills)
                if (value.m_configId == *instance.m_context.m_boundSkill) { selected = &value; break; }
        }
        return selected && vitals->mp >= pool->mpCost(*selected) * parameter(params);
    }
    case 10: case 161: case 162: {
        const auto* pool = world.ecs().tryGet<BuffPool>(condition == 161 ? entity : target);
        if (!pool) return false;
        for (const auto& value : pool->m_instances)
        {
            const auto* row = conf::Config::instance().find<conf::BuffConfig>(value.m_id);
            if (!row) continue;
            if (condition == 10 && std::find(params.begin(), params.end(), Fixed::fromInt(row->buffType)) != params.end()) return true;
            if (condition != 10 && (row->buffType == 3 || row->buffType == 4)) return true;
        }
        return false;
    }
    case 14: case 15: {
        const auto* pool = world.ecs().tryGet<SkillPool>(entity);
        if (!pool || !instance.m_context.m_currentSkill || !instance.m_context.m_frontSkill) return false;
        const auto* current = currentSkill(world, entity, instance);
        const SkillBase* front = nullptr;
        for (const auto& value : pool->m_skills)
            if (value.m_configId == *instance.m_context.m_frontSkill) { front = &value; break; }
        if (!current || !front) return false;
        if (condition == 14) return current->m_address.m_slot == front->m_address.m_slot &&
            current->m_address.m_index == front->m_address.m_index + 1;
        return current->m_configId != front->m_configId && current->m_address.m_slot != front->m_address.m_slot;
    }
    case 112: case 113: case 114: case 115: {
        const auto* displacement = world.ecs().tryGet<Displacement>(condition % 2 == 0 ? target : entity);
        if (!displacement) return false;
        const auto speed = displacement->velocityScale;
        if (condition < 114) return speed.x > "1"_fx && speed.z > "1"_fx;
        return speed.x != Fixed{} && speed.x < "1"_fx && speed.z != Fixed{} && speed.z < "1"_fx;
    }
    case 122: case 123: case 241: case 242: {
        const auto subject = condition == 122 || condition == 242 ? target : entity;
        const auto* state = world.ecs().tryGet<RoleStatus>(subject);
        if (!state) return false;
        const bool air = state->isHitUp() || state->isHitDown();
        return condition <= 123 ? air : !(air || state->isGetUp() || state->isHitFloor());
    }
    case 150: case 151: {
        const auto* hit = world.ecs().tryGet<RoleHitState>(entity);
        const auto* effect = hit ? world.ecs().tryGet<EffectState>(hit->m_beHitEffect) : nullptr;
        const auto* row = effect ? conf::Config::instance().find<conf::EffectConfig>(effect->m_configId) : nullptr;
        return row && row->effectType == (condition == 150 ? 2 : 1);
    }
    case 215:
        for (const auto& rule : conf::Config::instance().table<conf::BuffRuleConfig>())
            if (rule.className == "BuffCutSakura") return byRule(world, entity, rule.id) != nullptr;
        return false;
    case 174: {
        const auto* identity = world.ecs().tryGet<RoleIdentity>(target);
        const auto* row = identity ? conf::Config::instance().find<conf::RoleConfig>(identity->roleId) : nullptr;
        if (!row) return false;
        for (const auto sign : row->roleTypeSign)
            if (sign != -1 && std::find(params.begin(), params.end(), Fixed::fromInt(sign)) != params.end()) return true;
        return false;
    }
    case 249: {
        const auto* pool = world.ecs().tryGet<SkillPool>(entity);
        if (!pool) return false;
        constexpr std::array<std::string_view, 20> slots{
            "A", "B", "C", "D", "E", "F", "Y", "G", "H", "", "", "", "I", "J", "K", "L", "M", "N", "O", "P"};
        for (const auto index : params)
        {
            const int32_t slot = static_cast<int32_t>(index.toInt());
            if (slot < 1 || slot > static_cast<int32_t>(slots.size()) || slots[slot - 1].empty()) continue;
            for (const auto& value : pool->m_skills)
                if (pool->m_slots.at(value.m_address.m_slot).m_name == slots[slot - 1] && value.m_cd > Fixed{}) return true;
        }
        return false;
    }
    case 175: {
        if (!instance.m_context.m_currentSkill) return false;
        const auto* pool = world.ecs().tryGet<SkillPool>(entity);
        const auto* selected = currentSkill(world, entity, instance);
        if (!pool || !selected) return false;
        constexpr std::array<std::string_view, 29> slots{
            "A", "B", "C", "D", "E", "F", "Y", "G", "H", "6+", "8+", "12+",
            "I", "J", "K", "1+", "2+", "3+", "4+", "5+", "7+", "9+", "10+", "11+", "L", "M", "N", "O", "P"};
        auto slot = std::string_view(pool->m_slots.at(selected->m_address.m_slot).m_name);
        if (slot == "A+") slot = "A";
        for (size_t index = 0; index < slots.size(); ++index)
            if (slots[index] == slot)
                return std::find(params.begin(), params.end(), Fixed::fromInt(static_cast<int32_t>(index + 1))) != params.end();
        return false;
    }
    case 9: case 11: case 12: case 13: {
        const auto* skill = currentSkill(world, entity, instance);
        if (!skill) return false;
        if (condition == 9) return skill->m_address.m_index > 1 && skill->m_address.m_step == 1;
        if (skill->m_slotCount <= 1) return false;
        if (condition == 11) return skill->m_address.m_index == 1;
        return condition == 12 ? skill->m_address.m_index == skill->m_slotCount :
                                skill->m_address.m_index != skill->m_slotCount;
    }
    case 100: case 101: case 116: case 117: {
        const auto subject = (condition == 100 || condition == 116) ? target : entity;
        const auto* pool = world.ecs().tryGet<SkillPool>(subject);
        if (!pool) return false;
        return condition < 116 ? pool->m_crazy : !pool->m_crazy;
    }
    case 102: case 103: case 104: case 105: case 106: case 107: case 246: case 247: {
        const auto subject = condition % 2 == 0 ? target : entity;
        const auto* state = world.ecs().tryGet<sim::ExtraState>(subject);
        if (!state) return false;
        const auto kind = condition == 104 || condition == 105 ? mugen::ExtraState::Invincible :
            condition == 106 || condition == 107 ? mugen::ExtraState::Stun : mugen::ExtraState::SuperArmor;
        return condition >= 246 ? !state->active(kind) : state->active(kind);
    }
    case 124: case 125: case 126: case 127: case 144: case 145: {
        const auto* vitals = world.ecs().tryGet<Vitals>(condition % 2 == 0 ? target : entity);
        if (!vitals) return false;
        if (condition <= 125) return ratioMatches(vitals->hp, vitals->maxHp, parameter(params));
        if (condition <= 127) return ratioMatches(vitals->mp, vitals->maxMp, parameter(params));
        return ratioMatches(vitals->ep, vitals->maxEp, parameter(params));
    }
    case 155: return instance.m_context.m_needUseTp >= parameter(params);
    case 171: case 172: {
        const auto* timers = world.ecs().tryGet<RoleTimers>(entity);
        return timers && (condition == 171 ? timers->m_movedSeconds : timers->m_standingSeconds) >= parameter(params);
    }
    case 243: {
        if (!instance.m_context.m_currentSkill) return false;
        const auto* pool = world.ecs().tryGet<SkillPool>(entity);
        const auto* skill = currentSkill(world, entity, instance);
        if (!pool || !skill) return false;
        const auto slot = pool->baseSlot(*skill);
        for (const auto value : params)
        {
            if (value == "-1"_fx) return true;
            if (value == "1"_fx && slot == "6+") return true;
            if (value == "2"_fx && (slot == "8+" || slot == "11+" || slot == "4+")) return true;
            if (value == "3"_fx && (slot == "12+" || slot == "1+")) return true;
        }
        return false;
    }
    case 154: case 163: case 164: case 165: case 166: case 167: case 168: case 169:
    case 239: case 240: {
        const bool self = condition == 154 || condition == 163 || condition == 166 || condition == 168 || condition == 239;
        const auto* vitals = world.ecs().tryGet<Vitals>(self ? entity : target);
        if (!vitals) return false;
        if (condition == 154 || condition == 165) return vitals->m_lostHp;
        if (condition == 163 || condition == 164) return vitals->m_recoveredHp;
        if (condition == 166 || condition == 167) return vitals->m_lostEp;
        if (condition == 168 || condition == 169) return vitals->m_recoveredEp;
        const auto threshold = parameter(params);
        return (threshold < Fixed{} && vitals->m_hpChangePercent <= threshold) ||
               (threshold > Fixed{} && vitals->m_hpChangePercent >= threshold);
    }
    case 198: case 258: {
        const auto* pool = world.ecs().tryGet<BuffPool>(condition == 198 ? entity : target);
        if (!pool) return false;
        for (const auto& value : pool->m_instances)
            if (value.m_id == parameter(params).toInt())
                return Fixed::fromInt(value.m_lifecycle.m_repeat) >= parameter(params, 1);
        return false;
    }
    case 232: case 233: case 238: case 248: {
        const auto value = condition == 232 ? instance.m_context.m_artifactId :
            condition == 233 ? instance.m_context.m_artifactIndex :
            condition == 238 ? instance.m_context.m_currentSkill : instance.m_context.m_actionId;
        if (!value) return false;
        if (condition <= 233 && parameter(params) == "-1"_fx) return true;
        return std::find(params.begin(), params.end(), Fixed::fromInt(*value)) != params.end();
    }
    case 256: case 257:
        if (!world.ecs().has<BuffPool>(condition == 256 ? target : entity)) return false;
        for (const auto rule : params)
            if (!byRule(world, condition == 256 ? target : entity, static_cast<int32_t>(rule.toInt()))) return false;
        return true;
    case 8: {
        const auto* external = world.ecs().tryGet<BuffExternalConditions>(entity);
        return external && parameter(params) <= Fixed::fromInt(external->m_comboEvaluate);
    }
    case 263: {
        const auto* external = world.ecs().tryGet<BuffExternalConditions>(entity);
        if (!external || !external->m_angerPartner) return false;
        const bool transformed = external->m_angerFreezeSeconds > Fixed{};
        return parameter(params) == "1"_fx ? transformed : !transformed;
    }
    // 原条件在全部匹配后返回空值，因此这两个编号始终不通过。
    case 259: case 260: return false;
    default:
        MG_LOG_E("Unknown buff condition: {}", condition);
        return false;
    }
}
}
// 条件按源顺序短路，参数不够时提供空数组而不读取越界。
bool BuffHolder::checkConditions(const BuffInstance& instance) const
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance.m_id);
    if (!config) return false;
    return checkConditions(instance, *config);
}
// 允许调用者携带待应用配置，避免测试或重置修改全局配置归档。
bool BuffHolder::checkConditions(const BuffInstance& instance, const conf::BuffConfig& config) const
{
    const std::vector<Fixed> empty;
    for (size_t index = 0; index < config.condition.size(); ++index)
        if (!checkOne(m_world, m_entity, instance, config.condition[index],
                      index < config.conditionParam.size() ? config.conditionParam[index] : empty)) return false;
    return true;
}
}
