#include "PartnerLocalState.h"

#include "ui/progress/FeatureProgressSync.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/PartnerTables.h"

#include <algorithm>

namespace gameui
{

PartnerLocalState& PartnerLocalState::instance()
{
    static PartnerLocalState s;
    return s;
}

void PartnerLocalState::ensureUnlocked()
{
    if (m_seeded && !m_byId.empty())
        return;

    const auto& table = mugen::conf::Config::instance().table<mugen::conf::PartnerConfig>();
    for (const auto& row : table)
    {
        auto& st = m_byId[row.id];
        if (st.configId == 0)
        {
            st.configId = row.id;
            st.level    = 1;
            st.star     = 0;
            st.unlocked = true;
        }
    }
    m_seeded = true;
    AXLOGI("PartnerLocalState: unlocked {} partners", m_byId.size());
}

std::vector<PartnerInstanceState> PartnerLocalState::list() const
{
    std::vector<PartnerInstanceState> out;
    out.reserve(m_byId.size());
    for (const auto& [_, st] : m_byId)
        out.push_back(st);
    std::sort(out.begin(), out.end(), [](const PartnerInstanceState& a, const PartnerInstanceState& b) {
        return a.configId < b.configId;
    });
    return out;
}

PartnerInstanceState* PartnerLocalState::find(int32_t configId)
{
    auto it = m_byId.find(configId);
    return it == m_byId.end() ? nullptr : &it->second;
}

const PartnerInstanceState* PartnerLocalState::find(int32_t configId) const
{
    auto it = m_byId.find(configId);
    return it == m_byId.end() ? nullptr : &it->second;
}

void PartnerLocalState::setActive(int32_t configId)
{
    if (configId != 0 && !find(configId))
        return;
    m_activeId = configId;
    AXLOGI("PartnerLocalState: active partner={}", configId);
    persistToServer();
}

void PartnerLocalState::addLevel(int32_t configId, int32_t delta)
{
    auto* st = find(configId);
    if (!st)
        return;
    const auto* cfg = mugen::conf::Config::instance().find<mugen::conf::PartnerConfig>(configId);
    const int32_t cap = cfg && cfg->levelMax > 0 ? cfg->levelMax : kMaxLevel;
    st->level = std::clamp(st->level + delta, 1, cap);
    persistToServer();
}

void PartnerLocalState::addStar(int32_t configId, int32_t delta)
{
    auto* st = find(configId);
    if (!st)
        return;
    const auto* cfg = mugen::conf::Config::instance().find<mugen::conf::PartnerConfig>(configId);
    const int32_t cap = cfg && cfg->starMax > 0 ? cfg->starMax : kMaxStar;
    st->star = std::clamp(st->star + delta, 0, cap);
    persistToServer();
}

void PartnerLocalState::loadFromServer(const PB::Types::CharacterInfo& character)
{
    ensureUnlocked();
    for (const auto& p : character.partners())
    {
        auto* st = find(p.config_id());
        if (!st)
            continue;
        st->level = std::max(1, p.level());
        st->star  = std::max(0, p.star());
    }
    m_activeId = character.active_partner_id();
}

void PartnerLocalState::persistToServer()
{
    FeatureProgressSync::instance().syncPartners();
}

}  // namespace gameui
