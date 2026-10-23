#pragma once

#include "PetTypes.h"

#include "net/game_types.pb.h"

#include <unordered_map>
#include <vector>

namespace gameui
{

// Dev/cheat local pet inventory: unlock-all, free level/star, active follow pet.
class PetLocalState
{
public:
    PetLocalState() = default;
    static PetLocalState& instance();

    void ensureUnlocked();
    void loadFromServer(const PB::Types::CharacterInfo& character);
    void persistToServer();

    std::vector<PetInstanceState> list() const;
    PetInstanceState* find(int32_t configId);
    const PetInstanceState* find(int32_t configId) const;

    int32_t activeId() const { return m_activeId; }
    void setActive(int32_t configId);

    void addLevel(int32_t configId, int32_t delta = 1);
    void addStar(int32_t configId, int32_t delta = 1);

    static constexpr int32_t kMaxLevel = 100;
    static constexpr int32_t kMaxStar  = 25;

private:
    std::unordered_map<int32_t, PetInstanceState> m_byId;
    int32_t m_activeId = 0;
    bool m_seeded      = false;
};

}  // namespace gameui
