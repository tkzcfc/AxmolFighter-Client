#include "Skill.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>
#include <limits>

namespace mugen::sim::skill
{
namespace
{
/// 检查配置状态编号是否匹配角色状态，-1 表示不限制。
bool matchStatus(const RoleStatus& role, int32_t value)
{
    constexpr std::array states{EntityRoleStatus::Hit, EntityRoleStatus::HitUp, EntityRoleStatus::HitDown,
        EntityRoleStatus::HitFloor, EntityRoleStatus::Wake, EntityRoleStatus::Attack};
    return value == -1 || (value > 0 && value <= static_cast<int32_t>(states.size()) &&
                           role.has(states[value - 1]));
}
/// 按配置条件编号验证自动施放前提。
bool checkCondition(World& world, Entity entity, const SkillAi& ai, int32_t condition)
{
    auto& ecs = world.ecs();
    const auto& row = conf::Config::instance().get<conf::SkillAiConfig>(ai.m_configId);
    const auto target = ecs.get<SkillPool>(entity).m_target;
    switch (condition)
    {
    case 1: // 横向距离范围。
    case 2: // 纵向距离范围。
    {
        const auto* other = ecs.tryGet<Transform>(target);
        if (!other) return false;
        const auto& self = ecs.get<Transform>(entity);
        auto distance = condition == 1 ? other->position.x - self.position.x : other->position.z - self.position.z;
        if (distance < Fixed{}) distance = -distance;
        const auto& range = condition == 1 ? row.oppDisX : row.oppDisZ;
        return range.size() >= 2 && Fixed::fromInt(range[0]) - ai.m_moveDistanceFrame <= distance &&
               distance <= Fixed::fromInt(range[1]) + ai.m_moveDistanceFrame;
    }
    case 3: // 目标状态。
    {
        const auto* other = ecs.tryGet<RoleStatus>(target);
        return other && matchStatus(*other, row.oppStatus);
    }
    case 4: // 目标连段阶段。
    case 5: // 目标当前技能。
    {
        if ((condition == 4 ? row.oppCombo : row.oppSkillId) == -1) return true;
        const auto* other = ecs.tryGet<SkillPool>(target);
        const auto* state = ecs.tryGet<RoleStatus>(target);
        if (!other || !state || !state->isAttack() || other->m_current == kNoSkill) return false;
        const auto& current = other->m_skills.at(other->m_current);
        return condition == 4 ? current.m_address.m_index > 1 : current.m_configId == row.oppSkillId;
    }
    case 6: // 自身生命百分比。
    {
        const auto& vitals = ecs.get<Vitals>(entity);
        if (vitals.maxHp <= Fixed{} || row.selfHp.size() < 2) return false;
        const auto percent = "100"_fx * vitals.hp / vitals.maxHp;
        return Fixed::fromInt(row.selfHp[0]) <= percent && percent <= Fixed::fromInt(row.selfHp[1]);
    }
    case 7: return matchStatus(ecs.get<RoleStatus>(entity), row.selfStatus); // 自身状态。
    default: return false;
    }
}
}
bool checkComposition(World& world, Entity entity, const SkillAi& ai)
{
    const auto& row = conf::Config::instance().get<conf::SkillAiConfig>(ai.m_configId);
    if (row.composition.size() < 2) return false;
    const bool all = std::all_of(row.composition[0].begin(), row.composition[0].end(), [&](int32_t c) {
        return checkCondition(world, entity, ai, c);
    });
    const bool any = std::any_of(row.composition[1].begin(), row.composition[1].end(), [&](int32_t c) {
        return c == -1 || checkCondition(world, entity, ai, c);
    });
    return all && any;
}
bool checkAi(World& world, Entity entity, SkillAi& ai)
{
    if (ai.m_configId == -1) return true;
    if (ai.m_loadCd != Fixed{} || (ai.m_useCount != -1 && ai.m_useCount <= 0) || ai.m_checkCd != Fixed{}) return false;
    if (!checkComposition(world, entity, ai)) return false;
    const auto& row = conf::Config::instance().get<conf::SkillAiConfig>(ai.m_configId);
    ai.m_checkCd = Fixed::fromInt(std::min(row.checkCd, std::numeric_limits<int64_t>::max() / Fixed::kOne));
    if (world.ecs().singleton<RngState>().random.randomF({}, "100"_fx) >= Fixed::fromInt(row.prob)) return false;
    if (ai.m_useCount > 0) --ai.m_useCount;
    return true;
}
}
