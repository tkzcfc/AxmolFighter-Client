#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/core/MacroDefinition.h"

namespace mugen::sim
{
// 按统一流程创建战斗世界并返回全部玩家实体，失败时返回空向量。
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

    // 出生玩家：按配置写入 SpawnRequest，随后立即在出生阶段实体化。
    for (const auto& playerSetup : setup.m_players)
    {
        if (playerSetup.m_camp == static_cast<int32_t>(EntityCamp::Neutral))
        {
            MG_LOG_E("Player slot {} cannot use the neutral camp", playerSetup.m_slot);
            MG_ASSERT(false && "Player camp must not be neutral");
            return {};
        }
        const CharacterClass characterClass = playerSetup.m_class != CharacterClass::kUnknown
                                                  ? playerSetup.m_class
                                                  : type_conversions::toCharacterClass(playerSetup.m_roleId);
        RoleSpawnParams params;
        params.m_roleId       = playerSetup.m_roleId;
        params.m_level        = playerSetup.m_level;
        params.m_aiDifficulty = playerSetup.m_aiDifficulty;
        params.m_playerSlot   = playerSetup.m_slot;
        params.m_isAi         = playerSetup.m_isAi;
        if (playerSetup.m_camp != 0)
            params.m_camp = playerSetup.m_camp;
        if (characterClass != CharacterClass::kUnknown)
            role::rules::spawnPlayer(world, characterClass, params);
        else
            role::rules::spawnRole(world, params);
    }
    life::rules::spawnFlush(world, SpawnStage::Room);

    if (!room::rules::enter(world, *room, setup.m_stageId, setup.m_copyId))
    {
        MG_LOG_E("Battle bootstrap failed to enter room {}", setup.roomId);
        return {};
    }
    life::rules::spawnFlush(world, SpawnStage::Room);

    // 按出生顺序收集玩家实体。
    std::vector<Entity> heroes;
    for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().m_entities)
        heroes.push_back(entity);
    static_cast<void>(setup);
    return heroes;
}
}  // namespace mugen::sim
