#include "GameMapSystem.h"
#include "PhysicsSystem.h"
#include "SoundSystem.h"
#include "mugen/ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"

NS_MG_BEGIN

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
    auto physicsSys  = MG_GET_SYSTEM(getECSManager(), PhysicsSystem);
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
        auto it                    = mapDataConfigs.find(gameMapComp->mapId);
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
