#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>

namespace mugen::sim
{
Fixed SkillPool::mpCost(const SkillBase& skill) const
{
    auto rate = skill.m_mpScale + m_mpScale - "1"_fx;
    if (skill.m_address.m_index > 1) rate += conf::kLinkSkillMpRate - "1"_fx;
    return Fixed::fromInt(conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId).mp) * rate;
}
Fixed SkillPool::epCost(const SkillBase& skill) const
{
    return Fixed::fromInt(conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId).ep) *
           skill.m_epScale * m_epScale + skill.m_epPlus + m_epPlus;
}
bool SkillBase::isCooling() const { return m_releaseCount == 0 && m_cd > Fixed{}; }
bool SkillBase::isPriority(const SkillBase& current) const
{
    const auto& config = conf::Config::instance();
    const auto a = config.get<conf::SkillAttackConfig>(m_configId).sorder;
    const auto b = config.get<conf::SkillAttackConfig>(current.m_configId).sorder;
    return a == -1 || b == -1 || a >= b;
}
bool SkillPool::isSuperPriority(int32_t next, int32_t current) const
{
    const auto& config = conf::Config::instance();
    const auto& a = config.get<conf::SkillAttackConfig>(m_skills.at(next).m_configId).sorderControlType;
    const auto& b = config.get<conf::SkillAttackConfig>(m_skills.at(current).m_configId).sorderControlType;
    return (m_allowInterrupt && std::find(b.begin(), b.end(), 1) != b.end()) ||
           (m_allowExtraInterrupt && std::find(a.begin(), a.end(), 2) != a.end());
}
void SkillPool::dealWithDirection(int32_t index)
{
    auto& skill = m_skills.at(index);
    if ((skill.m_group == 3 || skill.m_group == 4) && skill.m_controlAngle)
    {
        m_controlAngle = skill.m_controlAngle;
        skill.m_controlAngle.reset();
    }
    if (m_controlAngle)
    {
        /// 以 45 度扇区将控制角映射到输入方向编号。
        constexpr std::array<int32_t, 8> vectors{4, 2, 1, 3, 5, 8, 6, 7};
        const auto sector = ((*m_controlAngle / "22.5"_fx).floor().toInt() + 1) / 2;
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
bool SkillBase::canUpRelease() const
{
    const auto& row = conf::Config::instance().get<conf::SkillAttackConfig>(m_configId);
    return m_enoughRelease || (m_upTouch && m_skillTime < Fixed::fromInt(row.pressTime));
}
}  // namespace mugen::sim

namespace mugen::sim::skill
{
bool SkillCaster::isAllowCastInternal(int32_t index, bool autoCast)
{
    auto& ecs = m_world.ecs();
    auto& pool = ecs.get<SkillPool>(m_entity);
    auto& skill = pool.m_skills.at(index);
    const auto& row = conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId);
    const auto& state = ecs.get<RoleStatus>(m_entity);
    const auto slot = pool.baseSlot(skill);
    if (row.type == -1 && ((state.isTobeHit() && slot != "L") ||
                          (slot == "L" && !state.isCanBreak()))) return false;
    const auto& extra = ecs.get<ExtraState>(m_entity);
    if (!extra.isCanControl() || extra.active(mugen::ExtraState::Silence)) return false;
    if (!pool.m_slots[skill.m_address.m_slot].m_enabled || skill.isCooling()) return false;
    const auto& vitals = ecs.get<Vitals>(m_entity);
    if (pool.mpCost(skill) > vitals.mp || pool.epCost(skill) > vitals.ep || skill.m_crystal > pool.m_crystals) return false;
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
}  // namespace mugen::sim::skill
