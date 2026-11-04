#include "BattleBootstrap.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "room/RoomDirector.h"
#include "spawn/RoleSpawn.h"

namespace mugen::sim
{
// 以统一流程创建战斗世界并返回主控玩家实体，失败时返回无效实体。
Entity bootstrapBattle(World& world, const WorldSetup& setup)
{
    const bool battleMode = setup.mode == WorldMode::Battle;
    if (!battleMode)
    {
        MG_LOG_E("Battle bootstrap requires Battle mode");
        MG_ASSERT(battleMode && "Battle bootstrap requires Battle mode");
        return {};
    }
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(setup.roomId);
    if (!room)
    {
        MG_LOG_E("Room config not found: {}", setup.roomId);
        MG_ASSERT(room != nullptr && "Room config not found");
        return {};
    }
    world.init(setup);

    RoleSpawnParams player;
    player.roleId       = setup.m_playerRoleId;
    player.level        = setup.m_playerLevel;
    player.aiDifficulty = setup.m_playerAiDifficulty;
    player.playerSlot   = setup.m_playerSlot;
    player.isAi         = setup.m_playerIsAi;
    if (setup.m_playerCamp != 0)
        player.camp = setup.m_playerCamp;

    const CharacterClass characterClass = setup.m_playerClass != CharacterClass::kUnknown
                                              ? setup.m_playerClass
                                              : type_conversions::toCharacterClass(setup.m_playerRoleId);
    const Entity hero = characterClass != CharacterClass::kUnknown ? RoleSpawner(world).spawnPlayer(characterClass, player)
                                                                    : RoleSpawner(world).spawnRole(player);
    if (hero.value == 0)
    {
        MG_LOG_E("Battle bootstrap failed to spawn player");
        MG_ASSERT(hero.value != 0 && "Battle bootstrap failed to spawn player");
        return {};
    }
    // PVP 决斗按出生生命倍率统一缩放玩家生命与上限。
    if (setup.m_pvp)
    {
        auto& attributes    = world.ecs().get<Attributes>(hero);
        attributes.basic[4] *= setup.m_pvpHpScale;
        auto& vitals = world.ecs().get<Vitals>(hero);
        vitals.maxHp = attributes.maxHp();
        vitals.hp    = vitals.maxHp;
    }
    if (!room::RoomDirector(world).enter(*room, setup.m_stageId, setup.m_copyId))
    {
        MG_LOG_E("Battle bootstrap failed to enter room {}", setup.roomId);
        return {};
    }
    return hero;
}
}  // namespace mugen::sim
