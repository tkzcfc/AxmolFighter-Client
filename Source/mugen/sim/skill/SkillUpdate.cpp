#include "SkillCaster.h"
#include "SkillTriggers.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>
#include <limits>

namespace mugen::sim::skill
{
void dealWithCoolDown(SkillBase& skill)
{
    const auto cd = conf::Config::instance().get<conf::SkillAttackConfig>(skill.m_configId).cd;
    skill.m_coldMaxTime = Fixed::fromInt(std::clamp(cd, int64_t{0}, std::numeric_limits<int64_t>::max() / Fixed::kOne)) * skill.m_coldScale;
    skill.m_timing = true;
    if (!(skill.m_cd > Fixed{} && skill.m_releaseMax > 1)) skill.m_cd = skill.m_coldMaxTime;
}
void addColdTime(SkillBase& skill, Fixed time)
{
    time *= conf::kColdTimeSale;
    skill.m_timing = true;
    skill.m_cd += time;
    skill.m_coldMaxTime += time;
}
void addSkillReleaseCount(SkillBase& skill)
{
    skill.m_releaseCount = std::min(skill.m_releaseCount + 1, skill.m_releaseMax);
    if (skill.m_releaseCount < skill.m_releaseMax) dealWithCoolDown(skill);
}
void refreshSkillPipe(SkillBase& skill)
{
    skill.m_previousPipe = skill.m_releaseMax + 1;
    skill.m_pipe = skill.m_releaseMax;
    skill.m_expectedPipe = skill.m_releaseMax;
}
void SkillCaster::update()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool) return;
    const auto dt = m_world.ecs().singleton<WorldClock>().dtMs;
    for (size_t i = 0; i < pool->m_skills.size(); ++i)
    {
        auto& skill = pool->m_skills[i];
        if (skill.m_timing)
        {
            const bool cooling = isCooling(skill);
            skill.m_skillTime += dt;
            skill.m_cd = std::max(Fixed{}, skill.m_cd - dt);
            if (skill.m_cd == Fixed{})
            {
                dealWithCoolDown(skill);
                skill.m_cd = {};
                skill.m_timing = false;
                addSkillReleaseCount(skill);
                if (cooling)
                {
                    buff::trigger(m_world, m_entity, BFEvent::SkillColdEnd, {static_cast<int32_t>(i)});
                    skill.m_enoughRelease = false;
                }
            }
        }
        skill.m_ai.m_loadCd = std::max(Fixed{}, skill.m_ai.m_loadCd - dt);
        skill.m_ai.m_checkCd = std::max(Fixed{}, skill.m_ai.m_checkCd - dt);
        if (skill.m_groupTime > Fixed{})
        {
            skill.m_groupTime = std::max(Fixed{}, skill.m_groupTime - dt);
            if (skill.m_groupTime == Fixed{}) pool->m_group = 1;
        }
        if (skill.m_changeTime > Fixed{})
        {
            skill.m_changeTime = std::max(Fixed{}, skill.m_changeTime - dt);
            if (skill.m_changeTime == Fixed{}) pool->m_change = 1;
        }
    }
    for (auto& interval : pool->m_aiSkillCastInterval) interval = std::max(Fixed{}, interval - dt);
    if (pool->m_beforeTime > Fixed{})
    {
        pool->m_beforeTime = std::max(Fixed{}, pool->m_beforeTime - dt);
        if (pool->m_beforeTime == Fixed{}) pool->m_before = kNoSkill;
    }
    pool->m_coolingSlots.clear();
    std::erase_if(pool->m_running, [&](int32_t i) { return !isCooling(pool->m_skills.at(i)); });
    for (const auto i : pool->m_running)
    {
        const auto& skill = pool->m_skills.at(i);
        const auto slot = baseSlot(*pool, skill);
        /// 仅这三类基础槽位需要向表现层汇总冷却状态。
        if (slot != "B" && slot != "C" && slot != "D") continue;
        const auto code = skill.m_address.m_slot;
        if (std::find(pool->m_coolingSlots.begin(), pool->m_coolingSlots.end(), code) == pool->m_coolingSlots.end())
            pool->m_coolingSlots.push_back(code);
    }
}
void SkillCaster::updateCrazy()
{
    auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (!pool || !pool->m_crazy) return;
    constexpr std::array additions{conf::kCrazyDecayValueRole1, conf::kCrazyDecayValueRole2,
        conf::kCrazyDecayValueRole3, conf::kCrazyDecayValueRole4};
    const auto id = m_world.ecs().get<RoleIdentity>(m_entity).roleId;
    const auto extra = id >= 1 && id <= 4 ? additions[id - 1] : Fixed{};
    auto& vitals = m_world.ecs().get<Vitals>(m_entity);
    vitals.ep = std::max(Fixed{}, vitals.ep - m_world.ecs().singleton<WorldClock>().dtMs *
        (conf::kCrazyDecayBasicValue + extra) * pool->m_cpScale);
    if (vitals.ep == Fixed{} && pool->m_slotChanged)
    {
        pool->m_crazy = false;
        buff::trigger(m_world, m_entity, BFEvent::AfterCrazy);
    }
}
}  // namespace mugen::sim::skill
