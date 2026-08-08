#pragma once

#include <vector>
#include <string>

#include "ServerConfigModel.h"
#include "AccountModel.h"
#include "CharacterModel.h"
#include "InventoryModel.h"

namespace PB::Game
{
class LoginResp;
class FetchCharacterListResp;
class CreateCharacterResp;
class SelectCharacterResp;
}  // namespace PB::Game

namespace game::model
{

class GameSessionModel
{
public:
    ServerConfigModel serverConfig;  // 服务器配置（如最大角色数）
    AccountModel account;            // 当前登录的账号信息

    std::vector<CharacterModel> characters;  // 账号下所有角色列表
    int64_t selectedCharacterID = 0;         // 当前选中的角色ID

    CharacterModel selectedCharacter;  // 选中的角色详细信息
    InventoryModel selectedInventory;  // 选中角色的背包（装备+物品）

    void clear();

    void setFromLoginResp(const PB::Game::LoginResp& resp, const std::string& accountName);
    void setCharacterListFromResp(const PB::Game::FetchCharacterListResp& resp);
    void appendFromCreateResp(const PB::Game::CreateCharacterResp& resp);
    void setSelectedFromSelectResp(const PB::Game::SelectCharacterResp& resp);

    CharacterModel* findCharacter(int64_t characterID);
};

}  // namespace game::model
