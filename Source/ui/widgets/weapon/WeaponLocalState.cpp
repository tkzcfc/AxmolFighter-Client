#include "WeaponLocalState.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/ArmamentTables.h"

#include <algorithm>

namespace gameui
{

WeaponLocalState& WeaponLocalState::instance()
{
    static WeaponLocalState s;
    return s;
}

void WeaponLocalState::ensureUnlockedForOccupation(int32_t occupation)
{
    if (m_seededOccupation == occupation && !m_byId.empty())
        return;

    const auto& table = mugen::conf::Config::instance().table<mugen::conf::ArmamentConfig>();
    for (const auto& row : table)
    {
        if (row.occupation != occupation && row.occupation != 0)
            continue;
        auto& st = m_byId[row.id];
        if (st.configId == 0)
        {
            st.configId = row.id;
            st.level    = 1;
            st.star     = 0;
            st.grade    = 0;
            st.skinId   = row.skin;
            st.skillId  = row.comboSkill;
            st.unlocked = true;
        }
    }
    m_seededOccupation = occupation;
    AXLOGI("WeaponLocalState: unlocked {} armaments for occupation {}", m_byId.size(), occupation);
}

std::vector<WeaponInstanceState> WeaponLocalState::listForOccupation(int32_t occupation) const
{
    std::vector<WeaponInstanceState> out;
    out.reserve(m_byId.size());
    const auto& table = mugen::conf::Config::instance().table<mugen::conf::ArmamentConfig>();
    for (const auto& row : table)
    {
        if (row.occupation != occupation && row.occupation != 0)
            continue;
        auto it = m_byId.find(row.id);
        if (it != m_byId.end())
            out.push_back(it->second);
    }
    std::sort(out.begin(), out.end(), [](const WeaponInstanceState& a, const WeaponInstanceState& b) {
        return a.configId < b.configId;
    });
    return out;
}

WeaponInstanceState* WeaponLocalState::find(int32_t configId)
{
    auto it = m_byId.find(configId);
    return it == m_byId.end() ? nullptr : &it->second;
}

const WeaponInstanceState* WeaponLocalState::find(int32_t configId) const
{
    auto it = m_byId.find(configId);
    return it == m_byId.end() ? nullptr : &it->second;
}

void WeaponLocalState::setAppearance(int32_t configId)
{
    m_appearanceId = configId;
    AXLOGI("WeaponLocalState: set appearance armamentId={}", configId);
}

void WeaponLocalState::setSkill(int32_t configId, int32_t skillId)
{
    if (auto* st = find(configId))
    {
        st->skillId = skillId;
        AXLOGI("WeaponLocalState: set skill armamentId={} skillId={}", configId, skillId);
    }
}

void WeaponLocalState::addLevel(int32_t configId, int32_t delta)
{
    if (auto* st = find(configId))
    {
        st->level = std::clamp(st->level + delta, 1, kMaxLevel);
        AXLOGI("WeaponLocalState: level armamentId={} -> {} (no cost)", configId, st->level);
    }
}

void WeaponLocalState::addStar(int32_t configId, int32_t delta)
{
    if (auto* st = find(configId))
    {
        st->star = std::clamp(st->star + delta, 0, kMaxStar);
        AXLOGI("WeaponLocalState: star armamentId={} -> {} (no cost)", configId, st->star);
    }
}

void WeaponLocalState::addGrade(int32_t configId, int32_t delta)
{
    if (auto* st = find(configId))
    {
        st->grade = std::clamp(st->grade + delta, 0, kMaxGrade);
        AXLOGI("WeaponLocalState: grade armamentId={} -> {} (no cost)", configId, st->grade);
    }
}

void WeaponLocalState::setSkin(int32_t configId, int32_t skinId)
{
    if (auto* st = find(configId))
    {
        st->skinId = skinId;
        AXLOGI("WeaponLocalState: skin armamentId={} skinId={}", configId, skinId);
    }
}

}  // namespace gameui
