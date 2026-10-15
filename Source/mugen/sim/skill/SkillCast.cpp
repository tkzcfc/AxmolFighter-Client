#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/sim/trace/TraceSink.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim
{
void SkillPool::setSkillBase(int32_t index)
{
    if (index != kNoSkill) { m_allowInterrupt = false; m_allowExtraInterrupt = false; }
    m_before = m_current;
    m_current = index;
    if (m_before != kNoSkill && baseSlot(m_skills.at(m_before)) == "L")
    {
        m_current = kNoSkill;
        m_beforeTime = "1000"_fx;
    }
}
}  // namespace mugen::sim

namespace mugen::sim::skill
{
void SkillCaster::resetSkill()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill) return;
    const auto slot = pool->baseSlot(pool->m_skills.at(pool->m_current));
    for (auto& skill : pool->m_skills)
        if (pool->baseSlot(skill) == slot) skill.m_nextSkill = kNoSkill;
    pool->setSkillBase(kNoSkill);
}
void SkillCaster::forcedInterrupt()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill) return;
    auto& skill = pool->m_skills.at(pool->m_current);
    clearSkillEffectsByInterrupt(skill.m_configId);
    if (skill.m_changeTime > Fixed{}) { skill.m_changeTime = {}; pool->m_change = 1; }
    resetSkill();
}
bool SkillCaster::dealWithNextSkillBase()
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    if (pool.m_current == kNoSkill) return false;
    auto& current = pool.m_skills.at(pool.m_current);
    auto next = current.m_nextSkill;
    if (next == kNoSkill) return false;
    const auto& candidate = pool.m_skills.at(next);
    if (candidate.m_group == 2 && !pool.m_crazy)
    {
        next = getGivenSkill(pool, pool.baseSlot(candidate).substr(0, 1));
        if (next != kNoSkill) pool.m_skills[next].m_controlAngle = candidate.m_controlAngle;
    }
    if (next == kNoSkill) return false;
    pool.dealWithDirection(next);
    pool.setSkillBase(next);
    auto& target = pool.m_skills.at(next);
    target.m_previousPipe = target.m_pipe;
    target.m_pipe = target.m_expectedPipe;
    target.m_nextRun = false;
    target.m_nextSkill = kNoSkill;
    current.m_nextSkill = kNoSkill;
    current.m_toward = 1;
    current.m_actionCount = 0;
    current.m_vector = 1;
    return true;
}
void SkillCaster::castBegan(int32_t index)
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    auto& skill = pool.m_skills.at(index);
    skill.dealWithCoolDown();
    pool.m_crystals -= std::max(0, skill.m_crystal);
    if (!pool.m_crazy)
    {
        auto& vitals = m_world.ecs().get<Vitals>(m_entity);
        const auto mp = pool.mpCost(skill), ep = pool.epCost(skill);
        vitals.mp = std::clamp(vitals.mp - mp, Fixed{}, vitals.maxMp);
        vitals.ep = std::clamp(vitals.ep - ep, Fixed{}, vitals.maxEp);
        trigger(BFEvent::UseTp, {index, mp});
        trigger(BFEvent::UseEp, {index, ep});
    }
    pool.dealWithDirection(index);
    if (skill.m_address.m_step == 1)
        for (auto& other : pool.m_skills)
            if (other.m_address.m_slot == skill.m_address.m_slot && other.m_address.m_index == skill.m_address.m_index &&
                other.m_address.m_mode != skill.m_address.m_mode)
            {
                other.m_releaseCount = std::max(0, other.m_releaseCount - 1);
                other.dealWithCoolDown();
            }
    if (skill.m_address.m_index > 1 || skill.m_address.m_step > 1)
        for (size_t i = 0; i < pool.m_skills.size(); ++i)
        {
            auto& other = pool.m_skills[i];
            if (static_cast<int32_t>(i) != index && other.m_address.m_slot == skill.m_address.m_slot && other.m_address.m_index == 1)
                other.addColdTime(skill.m_cd);
        }
    /// 轮换管线，供后续步骤校验正确的施放顺序。
    skill.m_previousPipe = skill.m_pipe;
    skill.m_pipe = skill.m_expectedPipe;
    skill.m_expectedPipe = skill.m_previousPipe - 1;
    skill.m_releaseCount = std::max(0, skill.m_releaseCount - 1);
    trigger(BFEvent::SkillColdStart, {index});
    pool.m_running.push_back(index);
    const auto* change = conf::Config::instance().find<conf::SkillChangeSystemConfig>(skill.m_configId);
    if (change)
    {
        if (change->skillGroupChangeId != -1)
        { pool.m_group = change->skillGroupChangeId; skill.m_groupTime = Fixed::fromInt(change->skillGroupTime); }
        if (change->skillChangeId != -1)
        { pool.m_change = change->skillChangeId; skill.m_changeTime = Fixed::fromInt(change->skillChangeTime); }
    }
}
void SkillCaster::dealWithCastPipeBegan(int32_t index)
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    const auto& skill = pool.m_skills.at(index);
    if (pool.m_previous != kNoSkill && skill.m_address.m_step == 1)
    {
        const auto& previous = pool.m_skills.at(pool.m_previous);
        if (pool.baseSlot(previous) != pool.baseSlot(skill) || previous.m_address.m_index != skill.m_address.m_index ||
            previous.m_address.m_step >= skill.m_address.m_step)
            trigger(BFEvent::BeforeNextSkill, {index});
    }
    castBegan(index);
    trace::noteCast(m_world, m_entity, skill.m_configId, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_pipe);
    const auto first = getGivenSkill(pool, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_address.m_index, skill.m_address.m_mode, 1, true);
    trigger(BFEvent::BeforeCastSkill, {first});
    trigger(ability::SkillEvent::BeforeCastSkill, {first});
    pool.m_previous = index;
}
void SkillCaster::dealWithCastPipeEnded(int32_t index)
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    const auto& skill = pool.m_skills.at(index);
    trace::noteCastEnd(m_world, m_entity, skill.m_configId);
    const auto first = getGivenSkill(pool, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_address.m_index, skill.m_address.m_mode, 1, true);
    trigger(BFEvent::AfterCastSkill, {first});
    if (skill.m_address.m_step == skill.m_stepCount) trigger(BFEvent::AfterNextSkill, {index});
    trigger(ability::SkillEvent::AfterCastSkill, {first});
    if (pool.m_current == index) dealWithNextSkillBase();
}
}  // namespace mugen::sim::skill
