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
void applyLayerMetaToGameMap(GameMapComponent& mapComp, const LayerLoadResult& loaded)
{
    if (loaded.size.x > 1 && loaded.size.y > 1)
    {
        mapComp.mapWidth  = loaded.size.x;
        mapComp.mapHeight = loaded.size.y;
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
        mapComp.scope.x      = static_cast<int32_t>(minX);
        mapComp.scope.y      = static_cast<int32_t>(minY);
        mapComp.scope.width  = static_cast<int32_t>(maxX - minX);
        mapComp.scope.height = static_cast<int32_t>(maxY - minY);
    }
    else if (mapComp.mapWidth > 0 && mapComp.mapHeight > 0)
    {
        mapComp.scope.x      = 0;
        mapComp.scope.y      = 0;
        mapComp.scope.width  = mapComp.mapWidth;
        mapComp.scope.height = mapComp.mapHeight;
    }

    if (mapComp.spawnPoints.empty() && mapComp.scope.width > 0 && mapComp.scope.height > 0)
    {
        mapComp.spawnPoints.push_back(
            Vector2i{mapComp.scope.x + mapComp.scope.width / 2, mapComp.scope.y + mapComp.scope.height / 2});
    }

    mapComp.soundId = loaded.soundId;
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
    MG_ASSERT(!gameMapComp->layerFile.empty());

    // 客户端与服务器共用：从 .layer 解析 size / moveRange / soundId
    const LayerLoadResult meta = LayerLoader::load(gameMapComp->layerFile);
    applyLayerMetaToGameMap(*gameMapComp, meta);

    auto physicsSys = MG_GET_SYSTEM(getECSManager(), PhysicsSystem);
    if (physicsSys)
    {
        auto& scope          = gameMapComp->scope;
        physicsSys->mapMin.x = static_cast<float>(scope.x);
        physicsSys->mapMax.x = static_cast<float>(scope.x + scope.width);
        physicsSys->mapMin.y = static_cast<float>(scope.y);
        physicsSys->mapMax.y = static_cast<float>(scope.y + scope.height);
    }

    if (getECSManager()->isDeserialized())
    {
        return;
    }

    auto soundSys = MG_GET_SYSTEM(getECSManager(), SoundSystem);
    if (soundSys && gameMapComp->soundId > 0)
    {
        soundSys->playBgmById(gameMapComp->soundId);
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
