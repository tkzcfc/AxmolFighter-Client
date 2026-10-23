#include "FeatureProgressSync.h"

#include "ui/widgets/partner/PartnerLocalState.h"
#include "ui/widgets/pet/PetLocalState.h"
#include "ui/widgets/skill/SkillLocalState.h"
#include "ui/widgets/weapon/WeaponLocalState.h"

#include "AppContext.h"

namespace gameui
{

FeatureProgressSync* FeatureProgressSync::getInstance()
{
    auto* p = AppContext::get().featureProgressSync();
    AXASSERT(p, "FeatureProgressSync not owned by AppContext");
    return p;
}

void FeatureProgressSync::hydrateFromCharacter(const PB::Types::CharacterInfo& character)
{
    const int32_t occupation = character.class_id();

    auto& weapons = WeaponLocalState::instance();
    weapons.loadFromServer(occupation, character);

    auto& skills = SkillLocalState::instance();
    skills.loadFromServer(occupation, character);

    auto& pets = PetLocalState::instance();
    pets.loadFromServer(character);

    auto& partners = PartnerLocalState::instance();
    partners.loadFromServer(character);

    AXLOGI(
        "FeatureProgressSync: hydrated armaments={} skills={} pets={} partners={} appearance={} "
        "activePet={} activePartner={}",
        character.armaments_size(), character.skill_slots_size(), character.pets_size(),
        character.partners_size(), character.armament_appearance_id(), character.active_pet_id(),
        character.active_partner_id());

    // First login / empty DB: push unlock-all seed so progress persists across sessions.
    if (character.armaments_size() == 0)
        syncArmaments();
    if (character.skill_slots_size() == 0)
        syncSkillSlots();
    if (character.pets_size() == 0)
        syncPets();
    if (character.partners_size() == 0)
        syncPartners();
}

void FeatureProgressSync::syncFashionWear(const PB::Game::FashionWearReq& req)
{
    call(req, [](const PB::Game::FashionWearResp* resp, std::string_view err) {
        if (!resp)
        {
            AXLOGW("FashionWear sync failed: {}", err);
            return;
        }
        if (resp->code() != 0)
            AXLOGW("FashionWear sync code={} msg={}", resp->code(), resp->message());
        else
            AXLOGI("FashionWear sync ok ({} fashions)", resp->code());
    });
}

void FeatureProgressSync::syncArmaments()
{
    PB::Game::ArmamentSyncReq req;
    auto& state = WeaponLocalState::instance();
    for (const auto& st : state.snapshotAll())
    {
        auto* a = req.add_armaments();
        a->set_config_id(st.configId);
        a->set_level(st.level);
        a->set_star(st.star);
        a->set_grade(st.grade);
        a->set_skin_id(st.skinId);
        a->set_skill_id(st.skillId);
    }
    req.set_appearance_id(state.appearanceId());

    call(req, [](const PB::Game::ArmamentSyncResp* resp, std::string_view err) {
        if (!resp)
        {
            AXLOGW("ArmamentSync failed: {}", err);
            return;
        }
        if (resp->code() != 0)
            AXLOGW("ArmamentSync code={} msg={}", resp->code(), resp->message());
    });
}

void FeatureProgressSync::syncSkillSlots()
{
    PB::Game::SkillSlotsSyncReq req;
    for (const auto& b : SkillLocalState::instance().slotBindings())
    {
        auto* s = req.add_slots();
        s->set_slot(static_cast<int32_t>(b.slot));
        s->set_leaf_id(b.leafId);
    }
    call(req, [](const PB::Game::SkillSlotsSyncResp* resp, std::string_view err) {
        if (!resp)
        {
            AXLOGW("SkillSlotsSync failed: {}", err);
            return;
        }
        if (resp->code() != 0)
            AXLOGW("SkillSlotsSync code={} msg={}", resp->code(), resp->message());
    });
}

void FeatureProgressSync::syncPets()
{
    PB::Game::PetSyncReq req;
    auto& state = PetLocalState::instance();
    for (const auto& st : state.list())
    {
        auto* p = req.add_pets();
        p->set_config_id(st.configId);
        p->set_level(st.level);
        p->set_star(st.star);
    }
    req.set_active_pet_id(state.activeId());
    call(req, [](const PB::Game::PetSyncResp* resp, std::string_view err) {
        if (!resp)
        {
            AXLOGW("PetSync failed: {}", err);
            return;
        }
        if (resp->code() != 0)
            AXLOGW("PetSync code={} msg={}", resp->code(), resp->message());
    });
}

void FeatureProgressSync::syncPartners()
{
    PB::Game::PartnerSyncReq req;
    auto& state = PartnerLocalState::instance();
    for (const auto& st : state.list())
    {
        auto* p = req.add_partners();
        p->set_config_id(st.configId);
        p->set_level(st.level);
        p->set_star(st.star);
    }
    req.set_active_partner_id(state.activeId());
    call(req, [](const PB::Game::PartnerSyncResp* resp, std::string_view err) {
        if (!resp)
        {
            AXLOGW("PartnerSync failed: {}", err);
            return;
        }
        if (resp->code() != 0)
            AXLOGW("PartnerSync code={} msg={}", resp->code(), resp->message());
    });
}

}  // namespace gameui
