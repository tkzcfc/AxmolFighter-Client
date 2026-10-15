#include "SkillCaster.h"
#include "SkillTriggers.h"
#include "mugen/sim/World.h"
#include "mugen/sim/trace/TraceSink.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim::skill
{
void setSkillBase(SkillPool& pool, int32_t index)
{
    if (index != kNoSkill) { pool.m_allowInterrupt = false; pool.m_allowExtraInterrupt = false; }
    pool.m_before = pool.m_current;
    pool.m_current = index;
    if (pool.m_before != kNoSkill && baseSlot(pool, pool.m_skills.at(pool.m_before)) == "L")
    {
        pool.m_current = kNoSkill;
        pool.m_beforeTime = "1000"_fx;
    }
}
void SkillCaster::resetSkill()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill) return;
    const auto slot = baseSlot(*pool, pool->m_skills.at(pool->m_current));
    for (auto& skill : pool->m_skills)
        if (baseSlot(*pool, skill) == slot) skill.m_nextSkill = kNoSkill;
    setSkillBase(*pool, kNoSkill);
}
void SkillCaster::forcedInterrupt()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill) return;
    auto& skill = pool->m_skills.at(pool->m_current);
    effect::clearSkillEffectsByInterrupt(m_world, m_entity, skill.m_configId);
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
        next = getGivenSkill(pool, baseSlot(pool, candidate).substr(0, 1));
        if (next != kNoSkill) pool.m_skills[next].m_controlAngle = candidate.m_controlAngle;
    }
    if (next == kNoSkill) return false;
    dealWithDirection(pool, next);
    setSkillBase(pool, next);
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
    dealWithCoolDown(skill);
    pool.m_crystals -= std::max(0, skill.m_crystal);
    if (!pool.m_crazy)
    {
        auto& vitals = m_world.ecs().get<Vitals>(m_entity);
        const auto mp = mpCost(pool, skill), ep = epCost(pool, skill);
        vitals.mp = std::clamp(vitals.mp - mp, Fixed{}, vitals.maxMp);
        vitals.ep = std::clamp(vitals.ep - ep, Fixed{}, vitals.maxEp);
        buff::trigger(m_world, m_entity, BFEvent::UseTp, {index, mp});
        buff::trigger(m_world, m_entity, BFEvent::UseEp, {index, ep});
    }
    dealWithDirection(pool, index);
    if (skill.m_address.m_step == 1)
        for (auto& other : pool.m_skills)
            if (other.m_address.m_slot == skill.m_address.m_slot && other.m_address.m_index == skill.m_address.m_index &&
                other.m_address.m_mode != skill.m_address.m_mode)
            {
                other.m_releaseCount = std::max(0, other.m_releaseCount - 1);
                dealWithCoolDown(other);
            }
    if (skill.m_address.m_index > 1 || skill.m_address.m_step > 1)
        for (size_t i = 0; i < pool.m_skills.size(); ++i)
        {
            auto& other = pool.m_skills[i];
            if (static_cast<int32_t>(i) != index && other.m_address.m_slot == skill.m_address.m_slot && other.m_address.m_index == 1)
                addColdTime(other, skill.m_cd);
        }
    /// 轮换管线，供后续步骤校验正确的施放顺序。
    skill.m_previousPipe = skill.m_pipe;
    skill.m_pipe = skill.m_expectedPipe;
    skill.m_expectedPipe = skill.m_previousPipe - 1;
    skill.m_releaseCount = std::max(0, skill.m_releaseCount - 1);
    buff::trigger(m_world, m_entity, BFEvent::SkillColdStart, {index});
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
        if (baseSlot(pool, previous) != baseSlot(pool, skill) || previous.m_address.m_index != skill.m_address.m_index ||
            previous.m_address.m_step >= skill.m_address.m_step)
            buff::trigger(m_world, m_entity, BFEvent::BeforeNextSkill, {index});
    }
    castBegan(index);
    trace::TraceRecorder(m_world).noteCast(m_entity, skill.m_configId, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_pipe);
    const auto first = getGivenSkill(pool, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_address.m_index, skill.m_address.m_mode, 1, true);
    buff::trigger(m_world, m_entity, BFEvent::BeforeCastSkill, {first});
    ability::trigger(m_world, m_entity, ability::SkillEvent::BeforeCastSkill, {first});
    pool.m_previous = index;
}
void SkillCaster::dealWithCastPipeEnded(int32_t index)
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    const auto& skill = pool.m_skills.at(index);
    trace::TraceRecorder(m_world).noteCastEnd(m_entity, skill.m_configId);
    const auto first = getGivenSkill(pool, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_address.m_index, skill.m_address.m_mode, 1, true);
    buff::trigger(m_world, m_entity, BFEvent::AfterCastSkill, {first});
    if (skill.m_address.m_step == skill.m_stepCount) buff::trigger(m_world, m_entity, BFEvent::AfterNextSkill, {index});
    ability::trigger(m_world, m_entity, ability::SkillEvent::AfterCastSkill, {first});
    if (pool.m_current == index) dealWithNextSkillBase();
}
}  // namespace mugen::sim::skill
