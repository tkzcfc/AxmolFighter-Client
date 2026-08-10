#include "ui/battle/LocalBattleMode.h"

#include "AppContext.h"
#include "mugen/ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

using namespace mugen;

namespace gameui
{

namespace
{
// 单机城镇调试图：town 41 → city_newcity_xueyuan
constexpr int32_t LOCAL_MAP_ID = 41;
// 英雄 role 101（resSpineId=88101）
constexpr int32_t PILOT_ROLE_ID = 101;
}  // namespace

LocalBattleMode::LocalBattleMode(int32_t roomId) : m_roomId(roomId) {}

bool LocalBattleMode::init(mugen::GameWord* gameWord)
{
    m_gameWord = gameWord;
    if (!m_gameWord)
    {
        MG_LOG_E("LocalBattleMode: GameWord is null");
        return false;
    }

    const bool isDungeon = m_roomId > 0;
    m_gameWord->setMode(isDungeon ? GameWordMode::kBattle : GameWordMode::kTown);

    const int32_t mapId = isDungeon ? m_roomId : LOCAL_MAP_ID;
    if (!m_gameWord->loadMap(mapId))
    {
        MG_LOG_E("LocalBattleMode: loadMap({}) failed", mapId);
        return false;
    }

    if (!spawnLocalPlayer())
    {
        MG_LOG_E("LocalBattleMode: spawnLocalPlayer failed");
        return false;
    }

    MG_LOG_I("LocalBattleMode: map {} loaded with local player (dungeon={})", mapId, isDungeon);
    return true;
}

void LocalBattleMode::onUpdate(float delta)
{
    if (m_gameWord)
        m_gameWord->update(delta);
}

void LocalBattleMode::setInput(uint32_t slot, bool pressed)
{
    if (!m_gameWord)
        return;

    auto director     = m_gameWord->getDirector();
    auto directorComp = MG_GET_COMPONENT(director, DirectorComponent);
    if (!directorComp)
        return;

    auto localPlayer = m_gameWord->ecsManager.getEntity(directorComp->localPlayerEntityId);
    if (!localPlayer)
        return;

    auto inputComp = MG_GET_COMPONENT(localPlayer, InputComponent);
    if (!inputComp)
        return;

    if (pressed)
        MG_BIT_SET(inputComp->keyDown, 1 << slot);
    else
        MG_BIT_REMOVE(inputComp->keyDown, 1 << slot);
}

bool LocalBattleMode::spawnLocalPlayer()
{
    auto* config = Config::getInstance();

    auto director     = m_gameWord->getDirector();
    auto directorComp = MG_GET_COMPONENT(director, DirectorComponent);
    if (!directorComp)
    {
        MG_LOG_E("LocalBattleMode: director missing");
        return false;
    }

    auto* mapEntity = m_gameWord->ecsManager.getEntity(directorComp->mapEntityId);
    auto* mapComp   = mapEntity ? MG_GET_COMPONENT(mapEntity, GameMapComponent) : nullptr;
    if (!mapComp)
    {
        MG_LOG_E("LocalBattleMode: GameMapComponent missing");
        return false;
    }

    int32_t spawnX = mapComp->scope.x + mapComp->scope.width / 2;
    int32_t spawnY = mapComp->scope.y + mapComp->scope.height / 2;
    if (!mapComp->spawnPoints.empty())
    {
        spawnX = mapComp->spawnPoints.front().x;
        spawnY = mapComp->spawnPoints.front().y;
    }

    int64_t playerId      = 0;
    std::string_view name = "Player";
    int32_t roleId        = PILOT_ROLE_ID;

    if (auto* session = AppContext::get().gameSession())
    {
        if (session->selectedCharacter.characterID != 0)
        {
            roleId   = actor_spawner::resolvePlayableRoleId(session->selectedCharacter.classID);
            playerId = session->account.playerID;
            name     = session->selectedCharacter.name;
            if (!config->getRoleConfigById(roleId))
            {
                MG_LOG_E("LocalBattleMode: role {} missing, fallback to pilot role {}", roleId, PILOT_ROLE_ID);
                roleId = PILOT_ROLE_ID;
            }
        }
    }

    auto player = actor_spawner::spawnRolePlayerActor(
        &m_gameWord->ecsManager, roleId, spawnX, spawnY,
        actor_spawner::PlayerSpawnParams{static_cast<int32_t>(playerId), std::string(name)});
    if (!player)
    {
        MG_LOG_E("LocalBattleMode: spawnRolePlayerActor failed role={}", roleId);
        return false;
    }
    player->notifyEntityReady();
    m_gameWord->bindLocalPlayer(player->getId());
    MG_LOG_I("LocalBattleMode: spawned player role={} at ({},{})", roleId, spawnX, spawnY);
    return true;
}

}  // namespace gameui
