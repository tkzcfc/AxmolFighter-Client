#include "SkillLocalState.h"

#include "ui/progress/FeatureProgressSync.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/SkillTables.h"
#include "mugen/conf/TypeConversions.h"

#include <algorithm>

namespace gameui
{

SkillLocalState& SkillLocalState::instance()
{
    static SkillLocalState s;
    return s;
}

void SkillLocalState::seedLeaves(int32_t occupation)
{
    m_leaves.clear();
    const auto& trees = mugen::conf::Config::instance().table<mugen::conf::SkillTreeConfig>();
    const auto& config = mugen::conf::Config::instance();
    for (const auto& tree : trees)
    {
        if (tree.professionType != occupation)
            continue;
        auto addLeaf = [&](int32_t leafId) {
            if (leafId <= 0)
                return;
            const auto* leaf = config.find<mugen::conf::SkillLeafConfig>(leafId);
            if (!leaf || leaf->skillId <= 0)
                return;
            auto& st   = m_leaves[leafId];
            st.leafId  = leafId;
            st.skillId = leaf->skillId;
            st.unlocked = true;
        };
        for (int32_t id : tree.leafIds)
            addLeaf(id);
        for (int32_t id : tree.leafRubId)
            addLeaf(id);
        for (int32_t id : tree.leafBuffId)
            addLeaf(id);
    }
    AXLOGI("SkillLocalState: unlocked {} skill leaves for occupation {}", m_leaves.size(), occupation);
}

void SkillLocalState::seedSlotsFromOfflineKit(int32_t occupation)
{
    m_slotToLeaf.clear();
    const auto cls  = static_cast<mugen::CharacterClass>(occupation);
    const auto rows = mugen::sim::skill::SkillAssembly::offlineKitRows(cls);
    for (const auto& row : rows)
    {
        if (row.slot == mugen::EnumSkillSlot::None || row.leafId <= 0)
            continue;
        bool editable = false;
        for (size_t i = 0; i < kEditableSlotCount; ++i)
        {
            if (kEditableSlots[i].slot == row.slot)
            {
                editable = true;
                break;
            }
        }
        if (!editable)
            continue;
        if (m_leaves.find(row.leafId) == m_leaves.end())
            continue;
        m_slotToLeaf[static_cast<int32_t>(row.slot)] = row.leafId;
    }
}

void SkillLocalState::ensureUnlockedForOccupation(int32_t occupation)
{
    if (m_seededOccupation == occupation && !m_leaves.empty())
        return;

    m_seededOccupation = occupation;
    seedLeaves(occupation);
    seedSlotsFromOfflineKit(occupation);
}

std::vector<SkillLeafState> SkillLocalState::listLeaves() const
{
    std::vector<SkillLeafState> out;
    out.reserve(m_leaves.size());
    for (const auto& [_, st] : m_leaves)
        out.push_back(st);
    std::sort(out.begin(), out.end(), [](const SkillLeafState& a, const SkillLeafState& b) {
        return a.leafId < b.leafId;
    });
    return out;
}

const SkillLeafState* SkillLocalState::findLeaf(int32_t leafId) const
{
    auto it = m_leaves.find(leafId);
    return it == m_leaves.end() ? nullptr : &it->second;
}

std::vector<SkillSlotBinding> SkillLocalState::slotBindings() const
{
    std::vector<SkillSlotBinding> out;
    out.reserve(kEditableSlotCount);
    for (size_t i = 0; i < kEditableSlotCount; ++i)
    {
        SkillSlotBinding b = kEditableSlots[i];
        auto it            = m_slotToLeaf.find(static_cast<int32_t>(b.slot));
        if (it != m_slotToLeaf.end())
        {
            b.leafId = it->second;
            if (const auto* leaf = findLeaf(b.leafId))
                b.skillId = leaf->skillId;
        }
        out.push_back(b);
    }
    return out;
}

int32_t SkillLocalState::leafInSlot(mugen::EnumSkillSlot slot) const
{
    auto it = m_slotToLeaf.find(static_cast<int32_t>(slot));
    return it == m_slotToLeaf.end() ? 0 : it->second;
}

void SkillLocalState::assignSlot(mugen::EnumSkillSlot slot, int32_t leafId)
{
    if (leafId <= 0 || !findLeaf(leafId))
        return;
    // One leaf per slot; also clear other slots that already hold this leaf.
    for (auto it = m_slotToLeaf.begin(); it != m_slotToLeaf.end();)
    {
        if (it->second == leafId)
            it = m_slotToLeaf.erase(it);
        else
            ++it;
    }
    m_slotToLeaf[static_cast<int32_t>(slot)] = leafId;
    AXLOGI("SkillLocalState: slot {} <- leaf {}", static_cast<int>(slot), leafId);
    persistToServer();
}

void SkillLocalState::clearSlot(mugen::EnumSkillSlot slot)
{
    m_slotToLeaf.erase(static_cast<int32_t>(slot));
    persistToServer();
}

void SkillLocalState::resetSlotsToDefault()
{
    if (m_seededOccupation > 0)
        seedSlotsFromOfflineKit(m_seededOccupation);
    persistToServer();
}

std::vector<mugen::sim::skill::PlayerSkill> SkillLocalState::playerSkills() const
{
    std::vector<mugen::sim::skill::PlayerSkill> out;
    for (const auto& b : slotBindings())
    {
        if (b.leafId <= 0 || b.skillId <= 0)
            continue;
        mugen::sim::skill::PlayerSkill ps;
        ps.m_slot   = b.slot;
        ps.m_baseId = b.skillId;
        ps.m_rockId = -1;
        ps.m_crazy  = b.slot == mugen::EnumSkillSlot::Crazy;
        out.push_back(ps);
    }
    return out;
}

void SkillLocalState::loadFromServer(int32_t occupation, const PB::Types::CharacterInfo& character)
{
    ensureUnlockedForOccupation(occupation);
    if (character.skill_slots_size() > 0)
    {
        m_slotToLeaf.clear();
        for (const auto& s : character.skill_slots())
        {
            if (s.slot() <= 0)
                continue;
            m_slotToLeaf[s.slot()] = s.leaf_id();
        }
    }
}

void SkillLocalState::persistToServer()
{
    FeatureProgressSync::instance().syncSkillSlots();
}

}  // namespace gameui
