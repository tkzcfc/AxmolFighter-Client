#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>

namespace mugen::sim::skill
{
Fixed mpCost(const SkillPool& pool, const SkillBase& skill)
{
    auto rate = skill.m_mpScale + pool.m_mpScale - "1"_fx;
    if (skill.m_address.m_index > 1) rate += conf::kLinkSkillMpRate - "1"_fx;
    return Fixed::fromInt(conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId).mp) * rate;
}
Fixed epCost(const SkillPool& pool, const SkillBase& skill)
{
    return Fixed::fromInt(conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId).ep) *
           skill.m_epScale * pool.m_epScale + skill.m_epPlus + pool.m_epPlus;
}
bool isCooling(const SkillBase& skill) { return skill.m_releaseCount == 0 && skill.m_cd > Fixed{}; }
bool SkillCaster::isAllowCastInternal(int32_t index, bool autoCast)
{
    auto& ecs = m_world.ecs();
    auto& pool = ecs.get<SkillPool>(m_entity);
    auto& skill = pool.m_skills.at(index);
    const auto& row = conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId);
    const auto& state = ecs.get<RoleStatus>(m_entity);
    const auto slot = baseSlot(pool, skill);
    if (row.type == -1 && ((state.isTobeHit() && slot != "L") ||
                          (slot == "L" && !state.isCanBreak()))) return false;
    const auto& extra = ecs.get<ExtraState>(m_entity);
    if (!extra.isCanControl() || extra.active(mugen::ExtraState::Silence)) return false;
    if (!pool.m_slots[skill.m_address.m_slot].m_enabled || isCooling(skill)) return false;
    const auto& vitals = ecs.get<Vitals>(m_entity);
    if (mpCost(pool, skill) > vitals.mp || epCost(pool, skill) > vitals.ep || skill.m_crystal > pool.m_crystals) return false;
    if (slot == "E" && pool.m_crazy) return false;
    return !autoCast || checkAi(skill.m_ai);
}
bool SkillCaster::isAllowCast(int32_t index, bool autoCast)
{
    const bool allow = isAllowCastInternal(index, autoCast);
    const auto& ecs = m_world.ecs();
    const auto& state = ecs.get<RoleStatus>(m_entity);
    if (!allow && ecs.get<RoleIdentity>(m_entity).roleType == 1 && !autoCast &&
        !ecs.get<SkillPool>(m_entity).m_autoCast && ecs.get<Transform>(m_entity).position.y == Fixed{} &&
        (state.isIdle() || state.isMove()) &&
        !state.isTobeHit() && !state.isAttack())
        m_world.emit(m_entity, Tips{"body shake hint"});
    return allow;
}
bool isPriority(const SkillBase& next, const SkillBase& current)
{
    const auto& config = conf::Config::instance();
    const auto a = config.get<conf::SkillAttackConfig>(next.m_configId).sorder;
    const auto b = config.get<conf::SkillAttackConfig>(current.m_configId).sorder;
    return a == -1 || b == -1 || a >= b;
}
bool isSuperPriority(const SkillPool& pool, int32_t next, int32_t current)
{
    const auto& config = conf::Config::instance();
    const auto& a = config.get<conf::SkillAttackConfig>(pool.m_skills.at(next).m_configId).sorderControlType;
    const auto& b = config.get<conf::SkillAttackConfig>(pool.m_skills.at(current).m_configId).sorderControlType;
    return (pool.m_allowInterrupt && std::find(b.begin(), b.end(), 1) != b.end()) ||
           (pool.m_allowExtraInterrupt && std::find(a.begin(), a.end(), 2) != a.end());
}
void dealWithDirection(SkillPool& pool, int32_t index)
{
    auto& skill = pool.m_skills.at(index);
    if ((skill.m_group == 3 || skill.m_group == 4) && skill.m_controlAngle)
    {
        pool.m_controlAngle = skill.m_controlAngle;
        skill.m_controlAngle.reset();
    }
    if (pool.m_controlAngle)
    {
        /// 以 45 度扇区将控制角映射到输入方向编号。
        constexpr std::array<int32_t, 8> vectors{4, 2, 1, 3, 5, 8, 6, 7};
        const auto sector = ((*pool.m_controlAngle / "22.5"_fx).floor().toInt() + 1) / 2;
        skill.m_vector = vectors.at(static_cast<size_t>((sector % 8 + 8) % 8));
    }
    const int32_t toward = skill.m_vector > 5 ? skill.m_vector - 5 : skill.m_vector;
    const auto& actions = conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId).actionIds;
    if (toward > 0 && static_cast<size_t>(toward) <= actions.size() && !actions[toward - 1].empty() && actions[toward - 1][0] != -1)
    {
        skill.m_toward = toward;
        skill.m_actionCount = static_cast<int32_t>(actions[toward - 1].size());
        skill.m_vector = 1;
    }
    else { skill.m_toward = 1; skill.m_actionCount = 0; }
}
bool canUpRelease(const SkillBase& skill)
{
    const auto& row = conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId);
    return skill.m_enoughRelease || (skill.m_upTouch && skill.m_skillTime < Fixed::fromInt(row.pressTime));
}
}  // namespace mugen::sim::skill
