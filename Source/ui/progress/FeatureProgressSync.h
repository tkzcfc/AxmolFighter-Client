#pragma once

#include "net/NetAgent.h"
#include "net/client_game.pb.h"
#include "net/game_types.pb.h"

namespace gameui
{

// AppContext-owned NetAgent that hydrates LocalStates from SelectCharacter and
// pushes fashion/armament/skill/pet/partner progress to the lobby game server.
class FeatureProgressSync : public net::NetAgent
{
public:
    FeatureProgressSync() = default;

    // Facade → AppContext::featureProgressSync(); asserts AppContext exists.
    static FeatureProgressSync* getInstance();
    static FeatureProgressSync& instance() { return *getInstance(); }

    // Apply CharacterInfo progress fields into LocalStates (after select character).
    void hydrateFromCharacter(const PB::Types::CharacterInfo& character);

    void syncFashionWear(const PB::Game::FashionWearReq& req);
    void syncArmaments();
    void syncSkillSlots();
    void syncPets();
    void syncPartners();

};

}  // namespace gameui
