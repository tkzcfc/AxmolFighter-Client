#include "BattleBootstrap.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "room/RoomDirector.h"
#include "spawn/RoleSpawn.h"

#include <vector>

namespace mugen::sim
{
// 以统一流程创建战斗世界并返回全部玩家实体，失败时返回空向量。
std::vector<Entity> bootstrapBattle(World& world, const WorldSetup& setup)
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

    std::vector<Entity> heroes;
    heroes.reserve(setup.m_players.size());
    for (const auto& playerSetup : setup.m_players)
    {
        RoleSpawnParams player;
        player.roleId       = playerSetup.m_roleId;
        player.level        = playerSetup.m_level;
        player.aiDifficulty = playerSetup.m_aiDifficulty;
        player.playerSlot   = playerSetup.m_slot;
        player.isAi         = playerSetup.m_isAi;
        // 中立阵营不在任何角色的敌对掩码内，特效无法命中它，玩家必须分属 Our/OurFriend 与 Enemy/EnemyFriend。
        if (playerSetup.m_camp == static_cast<int32_t>(EntityCamp::Neutral))
        {
            MG_LOG_E("Player slot {} cannot use the neutral camp: effects never hit it", playerSetup.m_slot);
            MG_ASSERT(false && "Player camp must not be neutral");
            return {};
        }
        if (playerSetup.m_camp != 0)
            player.camp = playerSetup.m_camp;

        const CharacterClass characterClass = playerSetup.m_class != CharacterClass::kUnknown
                                                  ? playerSetup.m_class
                                                  : type_conversions::toCharacterClass(playerSetup.m_roleId);
        const Entity hero = characterClass != CharacterClass::kUnknown ? RoleSpawner(world).spawnPlayer(characterClass, player)
                                                                        : RoleSpawner(world).spawnRole(player);
        if (hero.value == 0)
        {
            MG_LOG_E("Battle bootstrap failed to spawn player slot {}", playerSetup.m_slot);
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
        heroes.push_back(hero);
    }
    if (!room::RoomDirector(world).enter(*room, setup.m_stageId, setup.m_copyId))
    {
        MG_LOG_E("Battle bootstrap failed to enter room {}", setup.roomId);
        return {};
    }
    return heroes;
}
}  // namespace mugen::sim
