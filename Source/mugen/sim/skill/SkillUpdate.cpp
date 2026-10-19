#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>
#include <limits>

namespace mugen::sim
{
void SkillBase::dealWithCoolDown()
{
    const auto cd = conf::Config::instance().get<conf::SkillAttackConfig>(m_configId).cd;
    m_coldMaxTime =
        Fixed::fromInt(std::clamp(cd, int64_t{0}, std::numeric_limits<int64_t>::max() / Fixed::kOne)) * m_coldScale;
    m_timing = true;
    if (!(m_cd > Fixed{} && m_releaseMax > 1))
        m_cd = m_coldMaxTime;
}
void SkillBase::addColdTime(Fixed time)
{
    time *= conf::kColdTimeSale;
    m_timing = true;
    m_cd += time;
    m_coldMaxTime += time;
}
void SkillBase::addSkillReleaseCount()
{
    m_releaseCount = std::min(m_releaseCount + 1, m_releaseMax);
    if (m_releaseCount < m_releaseMax)
        dealWithCoolDown();
}
void SkillBase::refreshSkillPipe()
{
    m_previousPipe = m_releaseMax + 1;
    m_pipe         = m_releaseMax;
    m_expectedPipe = m_releaseMax;
}
}  // namespace mugen::sim

namespace mugen::sim::skill
{
void SkillCaster::update()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool)
        return;
    const auto dt = m_world.ecs().singleton<WorldClock>().dtMs;
    for (size_t i = 0; i < pool->m_skills.size(); ++i)
    {
        auto& skill = pool->m_skills[i];
        if (skill.m_timing)
        {
            const bool cooling = skill.isCooling();
            skill.m_skillTime += dt;
            skill.m_cd = std::max(Fixed{}, skill.m_cd - dt);
            if (skill.m_cd == Fixed{})
            {
                skill.dealWithCoolDown();
                skill.m_cd     = {};
                skill.m_timing = false;
                skill.addSkillReleaseCount();
                if (cooling)
                {
                    trigger(BFEvent::SkillColdEnd, {static_cast<int32_t>(i)});
                    skill.m_enoughRelease = false;
                }
            }
        }
        skill.m_ai.m_loadCd  = std::max(Fixed{}, skill.m_ai.m_loadCd - dt);
        skill.m_ai.m_checkCd = std::max(Fixed{}, skill.m_ai.m_checkCd - dt);
        if (skill.m_groupTime > Fixed{})
        {
            skill.m_groupTime = std::max(Fixed{}, skill.m_groupTime - dt);
            if (skill.m_groupTime == Fixed{})
                pool->m_group = 1;
        }
        if (skill.m_changeTime > Fixed{})
        {
            skill.m_changeTime = std::max(Fixed{}, skill.m_changeTime - dt);
            if (skill.m_changeTime == Fixed{})
                pool->m_change = 1;
        }
    }
    if (pool->m_beforeTime > Fixed{})
    {
        pool->m_beforeTime = std::max(Fixed{}, pool->m_beforeTime - dt);
        if (pool->m_beforeTime == Fixed{})
            pool->m_before = kNoSkill;
    }
    pool->m_coolingSlots.clear();
    std::erase_if(pool->m_running, [&](int32_t i) { return !pool->m_skills.at(i).isCooling(); });
    for (const auto i : pool->m_running)
    {
        const auto& skill = pool->m_skills.at(i);
        const auto slot   = pool->baseSlot(skill);
        /// 仅这三类基础槽位需要向表现层汇总冷却状态。
        if (slot != "B" && slot != "C" && slot != "D")
            continue;
        const auto code = skill.m_address.m_slot;
        if (std::find(pool->m_coolingSlots.begin(), pool->m_coolingSlots.end(), code) == pool->m_coolingSlots.end())
            pool->m_coolingSlots.push_back(code);
    }
}
void SkillCaster::updateCrazy()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || !pool->m_crazy)
        return;
    constexpr std::array additions{conf::kCrazyDecayValueRole1, conf::kCrazyDecayValueRole2,
                                   conf::kCrazyDecayValueRole3, conf::kCrazyDecayValueRole4};
    const auto id    = m_world.ecs().get<RoleIdentity>(m_entity).roleId;
    const auto extra = id >= 1 && id <= 4 ? additions[id - 1] : Fixed{};
    auto& vitals     = m_world.ecs().get<Vitals>(m_entity);
    vitals.ep        = std::max(Fixed{}, vitals.ep - m_world.ecs().singleton<WorldClock>().dtMs *
                                                         (conf::kCrazyDecayBasicValue + extra) * pool->m_cpScale);
    if (vitals.ep == Fixed{} && pool->m_slotChanged)
    {
        pool->m_crazy = false;
        trigger(BFEvent::AfterCrazy);
    }
}
}  // namespace mugen::sim::skill
