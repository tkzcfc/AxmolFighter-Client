#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>
#include <limits>

namespace mugen::sim::skill
{
namespace
{
/// 将技能 AI 状态编号映射为对应的角色状态位；Any 不限制，未知编号返回 None。
EntityRoleStatus toRoleStatus(SkillAiStatus value)
{
    switch (value)
    {
    case SkillAiStatus::Hit: return EntityRoleStatus::Hit;
    case SkillAiStatus::HitUp: return EntityRoleStatus::HitUp;
    case SkillAiStatus::HitDown: return EntityRoleStatus::HitDown;
    case SkillAiStatus::HitFloor: return EntityRoleStatus::HitFloor;
    case SkillAiStatus::Wake: return EntityRoleStatus::Wake;
    case SkillAiStatus::Attack: return EntityRoleStatus::Attack;
    default: return EntityRoleStatus::None;
    }
}
/// 检查角色是否处于指定技能 AI 状态，Any 表示不限制。
bool matchStatus(const RoleStatus& role, SkillAiStatus value)
{
    if (value == SkillAiStatus::Any) return true;
    const auto status = toRoleStatus(value);
    return status != EntityRoleStatus::None && role.has(status);
}
}  // namespace
/// 按配置条件编号验证自动施放前提。
bool SkillCaster::_checkCondition(const SkillAi& ai, SkillAiCondition condition) const
{
    auto& ecs = m_world.ecs();
    const auto& row = conf::Config::instance().get<conf::SkillAiConfig>(ai.m_configId);
    const auto target = ecs.get<SkillPool>(m_entity).m_target;
    switch (condition)
    {
    case SkillAiCondition::OppDistanceX: // 横向距离范围。
    case SkillAiCondition::OppDistanceZ: // 纵向距离范围。
    {
        const auto* other = ecs.tryGet<Transform>(target);
        if (!other) return false;
        const auto& self = ecs.get<Transform>(m_entity);
        auto distance = condition == SkillAiCondition::OppDistanceX ? other->position.x - self.position.x
                                                                    : other->position.z - self.position.z;
        if (distance < Fixed{}) distance = -distance;
        const auto& range = condition == SkillAiCondition::OppDistanceX ? row.oppDisX : row.oppDisZ;
        return range.size() >= 2 && Fixed::fromInt(range[0]) - ai.m_moveDistanceFrame <= distance &&
               distance <= Fixed::fromInt(range[1]) + ai.m_moveDistanceFrame;
    }
    case SkillAiCondition::OppStatus: // 目标状态。
    {
        const auto* other = ecs.tryGet<RoleStatus>(target);
        return other && matchStatus(*other, static_cast<SkillAiStatus>(row.oppStatus));
    }
    case SkillAiCondition::OppCombo: // 目标连段阶段。
    case SkillAiCondition::OppSkill: // 目标当前技能。
    {
        if ((condition == SkillAiCondition::OppCombo ? row.oppCombo : row.oppSkillId) == -1) return true;
        const auto* other = ecs.tryGet<SkillPool>(target);
        const auto* state = ecs.tryGet<RoleStatus>(target);
        if (!other || !state || !state->isAttack() || other->m_current == kNoSkill) return false;
        const auto& current = other->m_skills.at(other->m_current);
        return condition == SkillAiCondition::OppCombo ? current.m_address.m_index > 1
                                                       : current.m_configId == row.oppSkillId;
    }
    case SkillAiCondition::SelfHpPercent: // 自身生命百分比。
    {
        const auto& vitals = ecs.get<Vitals>(m_entity);
        if (vitals.maxHp <= Fixed{} || row.selfHp.size() < 2) return false;
        const auto percent = "100"_fx * vitals.hp / vitals.maxHp;
        return Fixed::fromInt(row.selfHp[0]) <= percent && percent <= Fixed::fromInt(row.selfHp[1]);
    }
    case SkillAiCondition::SelfStatus: // 自身状态。
        return matchStatus(ecs.get<RoleStatus>(m_entity), static_cast<SkillAiStatus>(row.selfStatus));
    default: return false;
    }
}
bool SkillCaster::checkComposition(const SkillAi& ai) const
{
    const auto& row = conf::Config::instance().get<conf::SkillAiConfig>(ai.m_configId);
    if (row.composition.size() < 2) return false;
    const bool all = std::all_of(row.composition[0].begin(), row.composition[0].end(), [&](int32_t c) {
        return _checkCondition(ai, static_cast<SkillAiCondition>(c));
    });
    const bool any = std::any_of(row.composition[1].begin(), row.composition[1].end(), [&](int32_t c) {
        return c == -1 || _checkCondition(ai, static_cast<SkillAiCondition>(c));
    });
    return all && any;
}
bool SkillCaster::checkAi(SkillAi& ai)
{
    if (ai.m_configId == -1) return true;
    if (ai.m_loadCd != Fixed{} || (ai.m_useCount != -1 && ai.m_useCount <= 0) || ai.m_checkCd != Fixed{}) return false;
    if (!checkComposition(ai)) return false;
    const auto& row = conf::Config::instance().get<conf::SkillAiConfig>(ai.m_configId);
    ai.m_checkCd = Fixed::fromInt(std::min(row.checkCd, std::numeric_limits<int64_t>::max() / Fixed::kOne));
    if (m_world.ecs().singleton<RngState>().random.randomF({}, "100"_fx) >= Fixed::fromInt(row.prob)) return false;
    if (ai.m_useCount > 0) --ai.m_useCount;
    return true;
}
}  // namespace mugen::sim::skill
