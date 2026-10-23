#include "PetLocalState.h"

#include "AppContext.h"
#include "core/event/EventDispatcher.h"

#include "ui/progress/FeatureProgressSync.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/PetTables.h"

#include <algorithm>

namespace gameui
{

PetLocalState& PetLocalState::instance()
{
    auto* p = AppContext::get().petLocalState();
    AXASSERT(p, "PetLocalState not owned by AppContext");
    return *p;
}

void PetLocalState::ensureUnlocked()
{
    if (m_seeded && !m_byId.empty())
        return;

    const auto& table = mugen::conf::Config::instance().table<mugen::conf::PetConfig>();
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
    AXLOGI("PetLocalState: unlocked {} pets", m_byId.size());
}

std::vector<PetInstanceState> PetLocalState::list() const
{
    std::vector<PetInstanceState> out;
    out.reserve(m_byId.size());
    for (const auto& [_, st] : m_byId)
        out.push_back(st);
    std::sort(out.begin(), out.end(), [](const PetInstanceState& a, const PetInstanceState& b) {
        return a.configId < b.configId;
    });
    return out;
}

PetInstanceState* PetLocalState::find(int32_t configId)
{
    auto it = m_byId.find(configId);
    return it == m_byId.end() ? nullptr : &it->second;
}

const PetInstanceState* PetLocalState::find(int32_t configId) const
{
    auto it = m_byId.find(configId);
    return it == m_byId.end() ? nullptr : &it->second;
}

void PetLocalState::setActive(int32_t configId)
{
    if (configId != 0 && !find(configId))
        return;
    m_activeId = configId;
    AXLOGI("PetLocalState: active pet={}", configId);
    persistToServer();
    if (auto* ed = AppContext::get().eventDispatcher())
        ed->dispatch(game::GameEvent::PetFollowChanged);
}

void PetLocalState::addLevel(int32_t configId, int32_t delta)
{
    auto* st = find(configId);
    if (!st)
        return;
    const auto* cfg = mugen::conf::Config::instance().find<mugen::conf::PetConfig>(configId);
    const int32_t cap = cfg && cfg->levelMax > 0 ? cfg->levelMax : kMaxLevel;
    st->level = std::clamp(st->level + delta, 1, cap);
    persistToServer();
}

void PetLocalState::addStar(int32_t configId, int32_t delta)
{
    auto* st = find(configId);
    if (!st)
        return;
    const auto* cfg = mugen::conf::Config::instance().find<mugen::conf::PetConfig>(configId);
    const int32_t cap = cfg && cfg->starMax > 0 ? cfg->starMax : kMaxStar;
    st->star = std::clamp(st->star + delta, 0, cap);
    persistToServer();
}

void PetLocalState::loadFromServer(const PB::Types::CharacterInfo& character)
{
    ensureUnlocked();
    for (const auto& p : character.pets())
    {
        auto* st = find(p.config_id());
        if (!st)
            continue;
        st->level = std::max(1, p.level());
        st->star  = std::max(0, p.star());
    }
    m_activeId = character.active_pet_id();
}

void PetLocalState::persistToServer()
{
    FeatureProgressSync::instance().syncPets();
}

}  // namespace gameui
