#include "model/GameSessionModel.h"
#include "net/client_game.pb.h"

namespace game::model
{

namespace
{

EquipmentModel equipmentFromProto(const PB::Types::EquipmentInfo& eq)
{
    EquipmentModel em;
    em.id           = eq.id();
    em.configID     = eq.config_id();
    em.enhanceLevel = eq.enhance_level();
    em.refineLevel  = eq.refine_level();
    em.slot         = eq.slot();
    for (int j = 0; j < eq.enchant_props_size(); ++j)
    {
        const auto& p = eq.enchant_props(j);
        em.enchantProps.push_back({p.attr_id(), p.value()});
    }
    return em;
}

FashionModel fashionFromProto(const PB::Types::FashionInfo& f)
{
    FashionModel fm;
    fm.id       = f.id();
    fm.configID = f.config_id();
    fm.position = f.position();
    fm.worn     = f.worn();
    return fm;
}

CharacterModel characterFromProto(const PB::Types::CharacterInfo& c)
{
    CharacterModel cm;
    cm.characterID = c.character_id();
    cm.name        = c.name();
    cm.classID     = c.class_id();
    cm.gender      = c.gender();
    cm.level       = c.level();
    cm.exp         = c.exp();
    cm.gold        = c.gold();

    cm.defaultSkins.reserve(static_cast<size_t>(c.default_skins_size()));
    for (int i = 0; i < c.default_skins_size(); ++i)
    {
        const auto& s = c.default_skins(i);
        cm.defaultSkins.push_back({s.position(), s.res_fashion_id()});
    }

    cm.fashions.reserve(static_cast<size_t>(c.fashions_size()));
    for (int i = 0; i < c.fashions_size(); ++i)
        cm.fashions.push_back(fashionFromProto(c.fashions(i)));

    cm.equipments.reserve(static_cast<size_t>(c.equipments_size()));
    for (int i = 0; i < c.equipments_size(); ++i)
        cm.equipments.push_back(equipmentFromProto(c.equipments(i)));

    return cm;
}

}  // namespace

void GameSessionModel::clear()
{
    serverConfig = ServerConfigModel{};
    account      = AccountModel{};
    characters.clear();
    selectedCharacterID = 0;
    selectedCharacter   = CharacterModel{};
    selectedInventory.clear();
}

void GameSessionModel::setFromLoginResp(const PB::Game::LoginResp& resp, const std::string& accountName)
{
    account.playerID = resp.player_id();
    account.account  = accountName;
    account.nickname = resp.nickname();

    if (resp.has_server_config())
    {
        serverConfig.maxCharacterCount = resp.server_config().max_character_count();
    }

    if (resp.has_account_info())
    {
        account.playerID = resp.account_info().player_id();
        account.account  = resp.account_info().account();
        account.nickname = resp.account_info().nickname();
    }
}

void GameSessionModel::setCharacterListFromResp(const PB::Game::FetchCharacterListResp& resp)
{
    characters.clear();
    characters.reserve(static_cast<size_t>(resp.characters_size()));

    for (int i = 0; i < resp.characters_size(); ++i)
        characters.push_back(characterFromProto(resp.characters(i)));

    if (selectedCharacterID == 0 && !characters.empty())
    {
        selectedCharacterID = characters.front().characterID;
    }
}

void GameSessionModel::appendFromCreateResp(const PB::Game::CreateCharacterResp& resp)
{
    if (!resp.has_character())
    {
        return;
    }

    CharacterModel cm = characterFromProto(resp.character());
    characters.push_back(cm);
    selectedCharacterID = cm.characterID;
}

void GameSessionModel::setSelectedFromSelectResp(const PB::Game::SelectCharacterResp& resp)
{
    if (resp.has_character())
    {
        selectedCharacter   = characterFromProto(resp.character());
        selectedCharacterID = selectedCharacter.characterID;
    }

    selectedInventory.clear();
    if (!resp.has_inventory())
    {
        return;
    }

    const auto& inv = resp.inventory();
    selectedInventory.items.reserve(static_cast<size_t>(inv.items_size()));
    selectedInventory.equipments.reserve(static_cast<size_t>(inv.equipments_size()));
    selectedInventory.fashions.reserve(static_cast<size_t>(inv.fashions_size()));

    for (int i = 0; i < inv.items_size(); ++i)
    {
        const auto& it = inv.items(i);
        ItemModel item;
        item.id       = it.id();
        item.configID = it.config_id();
        item.count    = it.count();
        selectedInventory.items.push_back(item);
    }

    for (int i = 0; i < inv.equipments_size(); ++i)
        selectedInventory.equipments.push_back(equipmentFromProto(inv.equipments(i)));

    for (int i = 0; i < inv.fashions_size(); ++i)
        selectedInventory.fashions.push_back(fashionFromProto(inv.fashions(i)));
}

CharacterModel* GameSessionModel::findCharacter(int64_t characterID)
{
    for (auto& c : characters)
    {
        if (c.characterID == characterID)
        {
            return &c;
        }
    }
    return nullptr;
}

}  // namespace game::model
