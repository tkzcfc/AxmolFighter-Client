#pragma once

#include "SkillTypes.h"

#include "net/game_types.pb.h"
#include "mugen/sim/skill/Skill.h"

#include <unordered_map>
#include <vector>

namespace gameui
{

// Dev/cheat local skill inventory: unlock-all leaves for occupation, free slot loadout.
class SkillLocalState
{
public:
    static SkillLocalState& instance();

    void ensureUnlockedForOccupation(int32_t occupation);
    void loadFromServer(int32_t occupation, const PB::Types::CharacterInfo& character);
    void persistToServer();

    std::vector<SkillLeafState> listLeaves() const;
    const SkillLeafState* findLeaf(int32_t leafId) const;

    // Current slot bindings (editable subset).
    std::vector<SkillSlotBinding> slotBindings() const;
    int32_t leafInSlot(mugen::EnumSkillSlot slot) const;
    void assignSlot(mugen::EnumSkillSlot slot, int32_t leafId);
    void clearSlot(mugen::EnumSkillSlot slot);
    void resetSlotsToDefault();

    // PlayerSkill list for SkillAssembly::playerLoadout (future battle hookup).
    std::vector<mugen::sim::skill::PlayerSkill> playerSkills() const;

private:
    SkillLocalState() = default;

    void seedLeaves(int32_t occupation);
    void seedSlotsFromOfflineKit(int32_t occupation);

    int32_t m_seededOccupation = -1;
    std::unordered_map<int32_t, SkillLeafState> m_leaves;  // leafId -> state
    std::unordered_map<int32_t, int32_t> m_slotToLeaf;     // EnumSkillSlot -> leafId
};

}  // namespace gameui
