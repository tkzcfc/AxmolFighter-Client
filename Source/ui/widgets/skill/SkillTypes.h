#pragma once

#include "mugen/conf/GameDef.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gameui
{

// One unlocked skill-tree leaf (dev/cheat: all profession leaves unlocked).
struct SkillLeafState
{
    int32_t leafId  = 0;
    int32_t skillId = 0;
    bool unlocked   = true;
};

// One editable loadout slot (subset of EnumSkillSlot used by heiyue skill config).
struct SkillSlotBinding
{
    mugen::EnumSkillSlot slot = mugen::EnumSkillSlot::None;
    const char* label         = "";
    int32_t leafId            = 0;  // 0 = empty
    int32_t skillId           = 0;
};

// Editable slots mirrored from heiyue UiSkillConfigNew3 NodeIcon / NodeRub / NodeBuff.
inline constexpr SkillSlotBinding kEditableSlots[] = {
    {mugen::EnumSkillSlot::Attack, "Attack", 0, 0},
    {mugen::EnumSkillSlot::Sprint, "Sprint", 0, 0},
    {mugen::EnumSkillSlot::Dodge, "Dodge", 0, 0},
    {mugen::EnumSkillSlot::Crazy, "Crazy", 0, 0},
    {mugen::EnumSkillSlot::A, "A", 0, 0},
    {mugen::EnumSkillSlot::B, "B", 0, 0},
    {mugen::EnumSkillSlot::C, "C", 0, 0},
    {mugen::EnumSkillSlot::SA, "SA", 0, 0},
    {mugen::EnumSkillSlot::SB, "SB", 0, 0},
    {mugen::EnumSkillSlot::SC, "SC", 0, 0},
    {mugen::EnumSkillSlot::EA, "EA", 0, 0},
    {mugen::EnumSkillSlot::EB, "EB", 0, 0},
};

inline constexpr size_t kEditableSlotCount = sizeof(kEditableSlots) / sizeof(kEditableSlots[0]);

}  // namespace gameui
