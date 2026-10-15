#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/Role.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>

namespace mugen::sim::skill
{
std::string SkillAssembly::eventSlot(ControlEvent event)
{
    const auto value = static_cast<int32_t>(event);
    /// 控制事件编码连续排列，转换为配置使用的槽位名。
    if (event >= ControlEvent::CTR_ATKA && event <= ControlEvent::CTR_ATKQ)
        return std::string(1, static_cast<char>('A' + value - static_cast<int32_t>(ControlEvent::CTR_ATKA)));
    if (event >= ControlEvent::CTR_JOY1 && event <= ControlEvent::CTR_JOY12)
        return std::to_string(value - static_cast<int32_t>(ControlEvent::CTR_JOY1) + 1) + '+';
    if (event >= ControlEvent::CTR_JOYB && event <= ControlEvent::CTR_JOYD)
        return std::string(1, static_cast<char>('B' + value - static_cast<int32_t>(ControlEvent::CTR_JOYB))) + "_2";
    return {};
}
bool SkillCaster::presetSkill(std::string_view slot, bool autoCast)
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    const auto next = pool.getFightSkill(slot);
    if (next == kNoSkill) return false;
    auto& candidate = pool.m_skills.at(next);
    candidate.m_upTouch = false;
    candidate.m_skillTime = {};
    if (!isAllowCast(next, autoCast)) return false;
    if (candidate.m_group == 3 || candidate.m_group == 4) candidate.m_controlAngle = pool.m_controlAngle;
    trigger(BFEvent::BeforePrepareSkill, {next});
    if (pool.m_current == kNoSkill)
    {
        pool.setSkillBase(next);
        pool.m_slotChanged = true;
        return true;
    }
    auto& current = pool.m_skills.at(pool.m_current);
    const bool same = slot == pool.baseSlot(current);
    if (pool.m_allowInterrupt && candidate.isPriority(current)) pool.setSkillBase(next);
    else
    {
        if (same && slot == "F") return false;
        const auto currentSlot = pool.baseSlot(current);
        for (auto& other : pool.m_skills)
            if (pool.baseSlot(other) == currentSlot && other.m_address.m_index == current.m_address.m_index &&
                other.m_address.m_mode == current.m_address.m_mode && other.m_address.m_step == current.m_address.m_step)
                other.m_nextSkill = next;
    }
    pool.m_slotChanged = !same;
    return true;
}
bool SkillCaster::dealWithButton(ControlEvent event, bool up, bool autoCast)
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    auto slot = SkillAssembly::eventSlot(event);
    if (slot.empty()) return false;
    if (up)
    {
        auto next = pool.getFightSkill(slot);
        if (next == kNoSkill) next = pool.m_current;
        if (next != kNoSkill) pool.m_skills[next].m_upTouch = true;
        return false;
    }
    if (slot == "A" && pool.m_canSprint)
    {
        const bool sprint = pool.m_current != kNoSkill && pool.baseSlot(pool.m_skills[pool.m_current]) == "Y" &&
                            !pool.m_skills[pool.m_current].m_finally;
        if (sprint || m_world.ecs().get<RoleStatus>(m_entity).isRun()) slot = "Y";
    }
    if (event >= ControlEvent::CTR_JOY1 && event <= ControlEvent::CTR_JOY12)
    {
        const auto joy = getGivenSkill(pool, pool.m_crazy ? slot + '+' : slot);
        if (joy == kNoSkill) slot = "A";
        else
        {
            trigger(BFEvent::BeforePrepareSkill, {joy});
            if (!isAllowCast(joy)) slot = "A";
        }
    }
    if (slot == "E" && (pool.m_crazy || (pool.m_current != kNoSkill && pool.baseSlot(pool.m_skills[pool.m_current]) == "E"))) return false;
    if (pool.m_crazy && getGivenSkill(pool, slot + '+') != kNoSkill) slot += '+';
    if (!m_world.ecs().get<RoleStatus>(m_entity).isAttack()) resetSkill();
    return presetSkill(slot, autoCast);
}
bool SkillCaster::dealButtonEvent(ControlEvent event, bool up, bool autoCast)
{
    const auto& state = m_world.ecs().get<RoleStatus>(m_entity);
    if (state.isDeath() || m_world.ecs().get<Vitals>(m_entity).hp <= Fixed{} ||
        !m_world.ecs().get<ExtraState>(m_entity).isCanControl() || state.isJostled()) return false;
    if (m_world.ecs().get<SkillPool>(m_entity).m_angerRecover) return false;
    return dealWithButton(event, up, autoCast) && Role(m_world, m_entity).changeStatus(EntityRoleStatus::Attack);
}
void SkillCaster::interruptRun()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill) return;
    auto& current = pool->m_skills[pool->m_current];
    const auto& row = conf::Config::instance().get<conf::SkillAttackConfig>(current.m_configId);
    if (row.sorder == -1 || conf::kRunSorder >= row.sorder ||
        (pool->m_allowInterrupt && std::find(row.sorderControlType.begin(), row.sorderControlType.end(), 1) != row.sorderControlType.end()))
    { current.m_nextRun = true; current.m_nextSkill = kNoSkill; }
}
}  // namespace mugen::sim::skill
