#include "GameMapSystem.h"
#include "PhysicsSystem.h"
#include "SoundSystem.h"
#include "mugen/ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/LayerLoader.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{
void applyLayerMetaToMapConfig(MapConfig& cfg, const LayerLoadResult& loaded)
{
    if (loaded.size.x > 1 && loaded.size.y > 1)
    {
        cfg.mapWidth  = loaded.size.x;
        cfg.mapHeight = loaded.size.y;
    }

    if (!loaded.moveRanges.empty())
    {
        float minX = loaded.moveRanges[0].x;
        float minY = loaded.moveRanges[0].y;
        float maxX = minX + loaded.moveRanges[0].width;
        float maxY = minY + loaded.moveRanges[0].height;
        for (size_t i = 1; i < loaded.moveRanges.size(); ++i)
        {
            const auto& r = loaded.moveRanges[i];
            minX          = std::min(minX, r.x);
            minY          = std::min(minY, r.y);
            maxX          = std::max(maxX, r.x + r.width);
            maxY          = std::max(maxY, r.y + r.height);
        }
        cfg.scope.x      = static_cast<int32_t>(minX);
        cfg.scope.y      = static_cast<int32_t>(minY);
        cfg.scope.width  = static_cast<int32_t>(maxX - minX);
        cfg.scope.height = static_cast<int32_t>(maxY - minY);
    }
    else if (cfg.mapWidth > 0 && cfg.mapHeight > 0)
    {
        cfg.scope.x      = 0;
        cfg.scope.y      = 0;
        cfg.scope.width  = cfg.mapWidth;
        cfg.scope.height = cfg.mapHeight;
    }

    if (cfg.spawnPoints.empty() && cfg.scope.width > 0 && cfg.scope.height > 0)
    {
        cfg.spawnPoints.push_back(Vector2i{cfg.scope.x + cfg.scope.width / 2, cfg.scope.y + cfg.scope.height / 2});
    }
}
}  // namespace

GameMapSystem::GameMapSystem() {}
GameMapSystem::~GameMapSystem() {}

void GameMapSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, GameMapComponent);
}

void GameMapSystem::onEntityAdded(Entity* entity)
{
    auto gameMapComp = MG_GET_COMPONENT(entity, GameMapComponent);
    MG_ASSERT(gameMapComp->mapConfig != nullptr);

    // 客户端与服务器共用：从 .layer 解析 size / moveRange
    if (!getECSManager()->isDeserialized() && !gameMapComp->mapConfig->layerFile.empty())
    {
        const LayerLoadResult meta = LayerLoader::load(gameMapComp->mapConfig->layerFile);
        applyLayerMetaToMapConfig(*const_cast<MapConfig*>(gameMapComp->mapConfig), meta);
    }

    auto physicsSys = MG_GET_SYSTEM(getECSManager(), PhysicsSystem);
    if (physicsSys)
    {
        auto& scope          = gameMapComp->mapConfig->scope;
        physicsSys->mapMin.x = static_cast<float>(scope.x);
        physicsSys->mapMax.x = static_cast<float>(scope.x + scope.width);
        physicsSys->mapMin.y = static_cast<float>(scope.y);
        physicsSys->mapMax.y = static_cast<float>(scope.y + scope.height);
    }

    if (getECSManager()->isDeserialized())
    {
        return;
    }

    // 播放地图背景音乐（优先 MapDataConfig.soundId）
    auto soundSys = MG_GET_SYSTEM(getECSManager(), SoundSystem);
    if (soundSys)
    {
        const auto& mapDataConfigs = Config::getInstance()->mapDataConfigs;
        const int32_t mapDataKey   = gameMapComp->mapDataId > 0 ? gameMapComp->mapDataId : gameMapComp->mapId;
        auto it                    = mapDataConfigs.find(mapDataKey);
        if (it != mapDataConfigs.end())
        {
            soundSys->playBgmById(it->second.soundId);
        }
    }

    const auto* roomConfig = Config::getInstance()->getRoomConfigById(gameMapComp->mapId);
    if (!roomConfig)
    {
        return;
    }

    for (const auto& monster : roomConfig->monsters)
    {
        actor_spawner::ActorSpawnParams params;
        params.category = EntityCategory::kMonster;
        auto monsterActor =
            actor_spawner::spawnRoleActor(getECSManager(), monster.monsterId, monster.posX, monster.posZ, params);
        if (!monsterActor)
        {
            continue;
        }
        monsterActor->notifyEntityReady();
    }
}

void GameMapSystem::onEntityRemoved(Entity* entity) {}

NS_MG_END
