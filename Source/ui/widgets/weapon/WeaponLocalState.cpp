#include "WeaponLocalState.h"

#include "AppContext.h"
#include "core/event/EventDispatcher.h"

#include "ui/progress/FeatureProgressSync.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/ArmamentTables.h"

#include <algorithm>

namespace gameui
{

WeaponLocalState& WeaponLocalState::instance()
{
    auto* p = AppContext::get().weaponLocalState();
    AXASSERT(p, "WeaponLocalState not owned by AppContext");
    return *p;
}

void WeaponLocalState::ensureUnlockedForOccupation(int32_t occupation)
{
    if (m_seededOccupation == occupation && !m_byId.empty())
        return;

    // Reseed when occupation changes so skill/appearance state stays occupation-scoped.
    if (m_seededOccupation != occupation)
    {
        m_byId.clear();
        m_appearanceId = 0;
    }

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
    persistToServer();
    if (auto* ed = AppContext::get().eventDispatcher())
        ed->dispatch(game::GameEvent::ArmamentAppearanceChanged);
}


void WeaponLocalState::setSkill(int32_t configId, int32_t skillId)
{
    if (auto* st = find(configId))
    {
        st->skillId = skillId;
        AXLOGI("WeaponLocalState: set skill armamentId={} skillId={}", configId, skillId);
    }
    persistToServer();
}


void WeaponLocalState::addLevel(int32_t configId, int32_t delta)
{
    if (auto* st = find(configId))
    {
        st->level = std::clamp(st->level + delta, 1, kMaxLevel);
        AXLOGI("WeaponLocalState: level armamentId={} -> {} (no cost)", configId, st->level);
    }
    persistToServer();
}


void WeaponLocalState::addStar(int32_t configId, int32_t delta)
{
    if (auto* st = find(configId))
    {
        st->star = std::clamp(st->star + delta, 0, kMaxStar);
        AXLOGI("WeaponLocalState: star armamentId={} -> {} (no cost)", configId, st->star);
    }
    persistToServer();
}


void WeaponLocalState::addGrade(int32_t configId, int32_t delta)
{
    if (auto* st = find(configId))
    {
        st->grade = std::clamp(st->grade + delta, 0, kMaxGrade);
        AXLOGI("WeaponLocalState: grade armamentId={} -> {} (no cost)", configId, st->grade);
    }
    persistToServer();
}


void WeaponLocalState::setSkin(int32_t configId, int32_t skinId)
{
    if (auto* st = find(configId))
    {
        st->skinId = skinId;
        AXLOGI("WeaponLocalState: skin armamentId={} skinId={}", configId, skinId);
    }
    persistToServer();
}


void WeaponLocalState::loadFromServer(int32_t occupation, const PB::Types::CharacterInfo& character)
{
    ensureUnlockedForOccupation(occupation);
    for (const auto& a : character.armaments())
    {
        auto* st = find(a.config_id());
        if (!st)
            continue;
        st->level   = std::max(1, a.level());
        st->star    = std::max(0, a.star());
        st->grade   = std::max(0, a.grade());
        st->skinId  = std::max(0, a.skin_id());
        st->skillId = std::max(0, a.skill_id());
    }
    if (character.armament_appearance_id() > 0)
        m_appearanceId = character.armament_appearance_id();
}

std::vector<WeaponInstanceState> WeaponLocalState::snapshotAll() const
{
    std::vector<WeaponInstanceState> out;
    out.reserve(m_byId.size());
    for (const auto& [_, st] : m_byId)
        out.push_back(st);
    return out;
}

void WeaponLocalState::persistToServer()
{
    FeatureProgressSync::instance().syncArmaments();
}

}  // namespace gameui
