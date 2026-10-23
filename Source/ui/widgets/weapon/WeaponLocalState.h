#pragma once

#include "WeaponTypes.h"

#include <optional>

namespace gameui
{

// Dev/cheat local inventory for 武装: unlock-all, free level/star/grade/skin/skill.
class WeaponLocalState
{
public:
    static WeaponLocalState& instance();

    // Ensure every ArmamentConfig row for occupation is unlocked at defaults.
    void ensureUnlockedForOccupation(int32_t occupation);

    std::vector<WeaponInstanceState> listForOccupation(int32_t occupation) const;
    WeaponInstanceState* find(int32_t configId);
    const WeaponInstanceState* find(int32_t configId) const;

    // Appearance armament id worn on character (0 = none). Local only.
    int32_t appearanceId() const { return m_appearanceId; }
    void setAppearance(int32_t configId);

    void setSkill(int32_t configId, int32_t skillId);
    void addLevel(int32_t configId, int32_t delta = 1);
    void addStar(int32_t configId, int32_t delta = 1);
    void addGrade(int32_t configId, int32_t delta = 1);
    void setSkin(int32_t configId, int32_t skinId);

    static constexpr int32_t kMaxLevel = 100;
    static constexpr int32_t kMaxStar  = 25;
    static constexpr int32_t kMaxGrade = 10;

private:
    WeaponLocalState() = default;

    std::unordered_map<int32_t, WeaponInstanceState> m_byId;
    int32_t m_appearanceId = 0;
    int32_t m_seededOccupation = -1;
};

}  // namespace gameui
