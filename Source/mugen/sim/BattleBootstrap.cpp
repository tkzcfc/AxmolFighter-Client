#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
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
    for (const auto& playerSetup : setup.players)
    {
        if (playerSetup.camp == static_cast<int32_t>(EntityCamp::Neutral))
        {
            MG_LOG_E("Player slot {} cannot use the neutral camp", playerSetup.slot);
            MG_ASSERT(false && "Player camp must not be neutral");
            return {};
        }
        RoleSpawnParams params;
        params.roleId       = playerSetup.roleId;
        params.level        = playerSetup.level;
        params.aiDifficulty = playerSetup.aiDifficulty;
        params.playerSlot   = playerSetup.slot;
        params.isAi         = playerSetup.isAi;
        if (playerSetup.camp != 0)
            params.camp = playerSetup.camp;
        role::rules::spawnRole(world, params);
    }
    life::rules::spawnFlush(world, SpawnStage::Room);

    // PVP 决斗按出生生命倍率统一缩放玩家生命与上限。
    if (setup.pvp)
    {
        for (const auto& [slot, hero] : world.ecs().singleton<PlayerSlots>().entities)
        {
            (void)slot;
            auto& stats    = world.ecs().get<StatsComponent>(hero);
            stats.base[4] *= setup.pvpHpScale;
            auto& health   = world.ecs().get<HealthComponent>(hero);
            health.maxHp   = stats.maxHp();
            health.hp      = health.maxHp;
            health.prevHp  = health.maxHp;
        }
    }

    if (!room::rules::enter(world, *room, setup.stageId, setup.copyId))
    {
        MG_LOG_E("Battle bootstrap failed to enter room {}", setup.roomId);
        return {};
    }
    life::rules::spawnFlush(world, SpawnStage::Room);

    // 按出生顺序收集玩家实体。
    std::vector<Entity> heroes;
    for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().entities)
        heroes.push_back(entity);
    return heroes;
}
}  // namespace mugen::sim
