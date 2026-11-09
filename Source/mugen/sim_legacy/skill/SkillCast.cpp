#include "SkillCaster.h"
#include "mugen/sim_legacy/World.h"
#include "mugen/sim_legacy/trace/TraceSink.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/sim_legacy/event/PresentationEvent.h"
#include <algorithm>

namespace mugen::sim_legacy
{
void SkillPool::setSkillBase(int32_t index)
{
    if (index != kNoSkill)
    {
        m_allowInterrupt      = false;
        m_allowExtraInterrupt = false;
    }
    m_before  = m_current;
    m_current = index;
    if (m_before != kNoSkill && baseSlot(m_skills.at(m_before)) == "L")
    {
        m_current    = kNoSkill;
        m_beforeTime = "1000"_fx;
    }
}
}  // namespace mugen::sim_legacy

namespace mugen::sim_legacy::skill
{
namespace
{
// 由角色与变身状态解析当前应使用的动作文件。
std::string resolvePartnerRageMotionFile(World& world, Entity entity, bool transformed)
{
    const int32_t roleId     = world.ecs().get<RoleIdentity>(entity).roleId;
    const auto& role         = conf::Config::instance().get<conf::RoleConfig>(roleId);
    const int32_t resSpineId = transformed && role.resSpineIdExt > 0 ? role.resSpineIdExt : role.resSpineId;
    if (resSpineId <= 0)
        return {};
    return AvatarPaths::motionFileFromSpine(conf::Config::instance().get<conf::ResSpineConfig>(resSpineId).fileName);
}

// 在施法管线开始时记录首段技能的怒气增量。
void beginPartnerRageCast(World& world, Entity entity, const SkillPool& pool, int32_t firstSkillIndex)
{
    auto* rage = world.ecs().tryGet<PartnerRageState>(entity);
    if (!rage)
        return;
    rage->m_pendingGain = {};
    rage->m_pendingGainActive = false;
    if (firstSkillIndex < 0 || static_cast<size_t>(firstSkillIndex) >= pool.m_skills.size())
        return;
    const int32_t firstSkillId = pool.m_skills[static_cast<size_t>(firstSkillIndex)].m_configId;
    if (const auto* skill = conf::Config::instance().find<conf::SkillAttackConfig>(firstSkillId))
    {
        rage->m_pendingGain = Fixed::fromInt(std::max(0, skill->rage));
        rage->m_pendingGainActive = rage->m_pendingGain > Fixed{};
    }
}

// 在怒气技能收束时进入变身并广播表现事件。
void finishPartnerRageCast(World& world, Entity entity, const SkillPool& pool, const SkillBase& skill)
{
    auto* rage = world.ecs().tryGet<PartnerRageState>(entity);
    if (!rage)
        return;
    rage->m_pendingGain       = {};
    rage->m_pendingGainActive = false;
    if (pool.baseSlot(skill) != "P")
        return;
    if (rage->m_transformed || rage->m_recoverPending || rage->m_anger < conf::kPartnerRageMaxAnger)
        return;
    rage->m_anger          = {};
    rage->m_freezeMs       = rage->m_maxFreezeMs;
    rage->m_transformed    = true;
    rage->m_recoverPending = rage->m_freezeMs <= Fixed{};
    clearSkillCooldownState(world, entity, false);
    setPartnerRageTransformState(world, entity, true);
}
}  // namespace

void clearSkillCooldownState(World& world, Entity entity, bool resetGroupChange)
{
    auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (!pool)
        return;
    pool->m_running.clear();
    pool->m_coolingSlots.clear();
    if (resetGroupChange)
    {
        pool->m_group  = 1;
        pool->m_change = 1;
    }
    for (auto& value : pool->m_skills)
    {
        value.m_cd           = {};
        value.m_timing       = false;
        value.m_releaseCount = value.m_releaseMax;
        if (resetGroupChange)
        {
            value.m_groupTime  = {};
            value.m_changeTime = {};
        }
    }
}

void setPartnerRageTransformState(World& world, Entity entity, bool transformed)
{
    auto* motion = world.ecs().tryGet<MotionState>(entity);
    if (!motion)
        return;
    if (motion->m_partnerRageTransformed != transformed)
        world.emit(entity, PartnerRageChanged{transformed});
    motion->m_partnerRageTransformed = transformed;
    if (const std::string motionFile = resolvePartnerRageMotionFile(world, entity, transformed); !motionFile.empty())
        motion->motionFile = motionFile;
}

void SkillCaster::resetSkill()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill)
        return;
    const auto slot = pool->baseSlot(pool->m_skills.at(pool->m_current));
    for (auto& skill : pool->m_skills)
        if (pool->baseSlot(skill) == slot)
            skill.m_nextSkill = kNoSkill;
    pool->setSkillBase(kNoSkill);
}
void SkillCaster::forcedInterrupt()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || pool->m_current == kNoSkill)
        return;
    auto& skill = pool->m_skills.at(pool->m_current);
    clearSkillEffectsByInterrupt(skill.m_configId);
    if (skill.m_changeTime > Fixed{})
    {
        skill.m_changeTime = {};
        pool->m_change     = 1;
    }
    resetSkill();
}
bool SkillCaster::dealWithNextSkillBase()
{
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    if (pool.m_current == kNoSkill)
        return false;
    auto& current = pool.m_skills.at(pool.m_current);
    auto next     = current.m_nextSkill;
    if (next == kNoSkill)
        return false;
    const auto& candidate = pool.m_skills.at(next);
    if (candidate.m_group == 2 && !pool.m_crazy)
    {
        next = getGivenSkill(pool, pool.baseSlot(candidate).substr(0, 1));
        if (next != kNoSkill)
            pool.m_skills[next].m_controlAngle = candidate.m_controlAngle;
    }
    if (next == kNoSkill)
        return false;
    pool.dealWithDirection(next);
    pool.setSkillBase(next);
    auto& target          = pool.m_skills.at(next);
    target.m_previousPipe = target.m_pipe;
    target.m_pipe         = target.m_expectedPipe;
    target.m_nextRun      = false;
    target.m_nextSkill    = kNoSkill;
    current.m_nextSkill   = kNoSkill;
    current.m_toward      = 1;
    current.m_actionCount = 0;
    current.m_vector      = 1;
    return true;
}
void SkillCaster::castBegan(int32_t index)
{
    auto& pool  = m_world.ecs().get<SkillPool>(m_entity);
    auto& skill = pool.m_skills.at(index);
    skill.dealWithCoolDown();
    pool.m_crystals -= std::max(0, skill.m_crystal);
    if (!pool.m_crazy)
    {
        auto& vitals  = m_world.ecs().get<Vitals>(m_entity);
        const auto mp = pool.mpCost(skill), ep = pool.epCost(skill);
        vitals.mp = std::clamp(vitals.mp - mp, Fixed{}, vitals.maxMp);
        vitals.ep = std::clamp(vitals.ep - ep, Fixed{}, vitals.maxEp);
        trigger(BFEvent::UseTp, {index, mp});
        trigger(BFEvent::UseEp, {index, ep});
    }
    pool.dealWithDirection(index);
    if (skill.m_address.m_step == 1)
        for (auto& other : pool.m_skills)
            if (other.m_address.m_slot == skill.m_address.m_slot &&
                other.m_address.m_index == skill.m_address.m_index && other.m_address.m_mode != skill.m_address.m_mode)
            {
                other.m_releaseCount = std::max(0, other.m_releaseCount - 1);
                other.dealWithCoolDown();
            }
    if (skill.m_address.m_index > 1 || skill.m_address.m_step > 1)
        for (size_t i = 0; i < pool.m_skills.size(); ++i)
        {
            auto& other = pool.m_skills[i];
            if (static_cast<int32_t>(i) != index && other.m_address.m_slot == skill.m_address.m_slot &&
                other.m_address.m_index == 1)
                other.addColdTime(skill.m_cd);
        }
    /// 轮换管线，供后续步骤校验正确的施放顺序。
    skill.m_previousPipe = skill.m_pipe;
    skill.m_pipe         = skill.m_expectedPipe;
    skill.m_expectedPipe = skill.m_previousPipe - 1;
    skill.m_releaseCount = std::max(0, skill.m_releaseCount - 1);
    trigger(BFEvent::SkillColdStart, {index});
    pool.m_running.push_back(index);
    const auto* change = conf::Config::instance().find<conf::SkillChangeSystemConfig>(skill.m_configId);
    if (change)
    {
        if (change->skillGroupChangeId != -1)
        {
            pool.m_group      = change->skillGroupChangeId;
            skill.m_groupTime = Fixed::fromInt(change->skillGroupTime);
        }
        if (change->skillChangeId != -1)
        {
            pool.m_change      = change->skillChangeId;
            skill.m_changeTime = Fixed::fromInt(change->skillChangeTime);
        }
    }
}
void SkillCaster::dealWithCastPipeBegan(int32_t index)
{
    auto& pool        = m_world.ecs().get<SkillPool>(m_entity);
    const auto& skill = pool.m_skills.at(index);
    if (pool.m_previous != kNoSkill && skill.m_address.m_step == 1)
    {
        const auto& previous = pool.m_skills.at(pool.m_previous);
        if (pool.baseSlot(previous) != pool.baseSlot(skill) || previous.m_address.m_index != skill.m_address.m_index ||
            previous.m_address.m_step >= skill.m_address.m_step)
            trigger(BFEvent::BeforeNextSkill, {index});
    }
    castBegan(index);
    trace::TraceRecorder(m_world).noteCast(m_entity, skill.m_configId, pool.m_slots[skill.m_address.m_slot].m_name,
                                           skill.m_pipe);
    const auto first = getGivenSkill(pool, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_address.m_index,
                                     skill.m_address.m_mode, 1, true);
    beginPartnerRageCast(m_world, m_entity, pool, first);
    trigger(BFEvent::BeforeCastSkill, {first});
    trigger(ability::SkillEvent::BeforeCastSkill, {first});
    pool.m_previous = index;
}
void SkillCaster::dealWithCastPipeEnded(int32_t index)
{
    auto& pool        = m_world.ecs().get<SkillPool>(m_entity);
    const auto& skill = pool.m_skills.at(index);
    trace::TraceRecorder(m_world).noteCastEnd(m_entity, skill.m_configId);
    const auto first = getGivenSkill(pool, pool.m_slots[skill.m_address.m_slot].m_name, skill.m_address.m_index,
                                     skill.m_address.m_mode, 1, true);
    finishPartnerRageCast(m_world, m_entity, pool, skill);
    trigger(BFEvent::AfterCastSkill, {first});
    if (skill.m_address.m_step == skill.m_stepCount)
        trigger(BFEvent::AfterNextSkill, {index});
    trigger(ability::SkillEvent::AfterCastSkill, {first});
    if (pool.m_current == index)
        dealWithNextSkillBase();
}
}  // namespace mugen::sim_legacy::skill
