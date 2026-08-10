#include "GameWord.h"
#include "mugen/Components.h"
#include "mugen/Systems.h"
#include "mugen/conf/Config.h"
#include "mugen/core/io/FileUtils.h"

#ifdef RUNTIME_IN_AXMOL
#    include "mugen/render/RenderObjectPool.h"
#endif

NS_MG_BEGIN

GameWord::GameWord() : m_director(nullptr), m_mode(GameWordMode::kBattle)
{
#ifdef RUNTIME_IN_AXMOL
    m_wordRootNode = nullptr;
#endif
}
GameWord::~GameWord()
{
    if (m_director)
    {
        m_director->destroy();
        m_director = nullptr;
    }
}

#ifdef RUNTIME_IN_AXMOL
bool GameWord::init(ax::Node* node, uint64_t randomSeed)
{
    m_wordRootNode = node;
#else
bool GameWord::init(uint64_t randomSeed)
{
#endif

    random.seed(randomSeed);

    ecsManager.setUserdata(this);

    // register components
#define X(COMPONENT_TYPE) ecsManager.registerComponent(#COMPONENT_TYPE, []() { return new COMPONENT_TYPE(); });
    COMPONENT_LIST
#undef X

    // register systems
#define X(SYSTEM_TYPE)                                     \
    ecsManager.registerSystem(#SYSTEM_TYPE, [](auto ecs) { \
        auto system = new SYSTEM_TYPE();                   \
        system->init(ecs);                                 \
        return system;                                     \
    });
    SYSTEM_LIST
#undef X

    // 此处服务端也需要添加上渲染系统,客户端才能完全反序列化出来
    ecsManager.addSystem("GameMapSystem");
    ecsManager.addSystem("GameMapRenderSystem");
    ecsManager.addSystem("AvatarSystem");
    ecsManager.addSystem("AvatarRenderSystem");
    ecsManager.addSystem("AttributeSystem");
    ecsManager.addSystem("InputSystem");
    ecsManager.addSystem("PhysicsSystem");
    ecsManager.addSystem("AISystem");
    ecsManager.addSystem("BuffSystem");
    ecsManager.addSystem("EffectLifeSystem");
    ecsManager.addSystem("CombatSystem");
    ecsManager.addSystem("BehaviorTreeSystem");
    ecsManager.addSystem("DisplacementSystem");
    ecsManager.addSystem("SoundSystem");

    MG_GET_SYSTEM((&ecsManager), SoundSystem)->setRandomSeed(random.next());

    m_director                          = ecsManager.newEntity();
    auto directorComp                   = MG_ADD_COMPONENT(m_director, DirectorComponent);
    directorComp->debugDrawCollisionBox = true;
    m_director->notifyEntityReady();

    return true;
}

void GameWord::update(float dt)
{
    ecsManager.update(static_cast<int32_t>(dt * 1000.0f));

#ifdef RUNTIME_IN_AXMOL
    RenderObjectPool::getInstance()->update(dt);
#endif
}

bool GameWord::loadMap(int32_t mapId)
{
    auto* config = Config::getInstance();

    // town / room / camp id → mapKey
    std::string mapKey;
    int32_t logicalId            = mapId;
    const TownConfig* townConfig = nullptr;
    const RoomConfig* roomConfig = nullptr;
    const CampConfig* campConfig = nullptr;

    auto townIt = config->townConfigs.find(mapId);
    if (townIt != config->townConfigs.end() && !townIt->second.mapKey.empty())
    {
        townConfig = &townIt->second;
        mapKey     = townConfig->mapKey;
    }
    else
    {
        auto roomIt = config->roomConfigs.find(mapId);
        if (roomIt != config->roomConfigs.end() && !roomIt->second.mapKey.empty())
        {
            roomConfig = &roomIt->second;
            mapKey     = roomConfig->mapKey;
        }
        else
        {
            auto campIt = config->campConfigs.find(mapId);
            if (campIt != config->campConfigs.end() && !campIt->second.mapKey.empty())
            {
                campConfig = &campIt->second;
                mapKey     = campConfig->mapKey;
            }
        }
    }

    if (mapKey.empty())
    {
        MG_LOG_E("GameWord::loadMap: no Town/Room/Camp mapKey for id={}", mapId);
        return false;
    }

    std::vector<Vector2i> spawnPoints;
    if (townConfig && (townConfig->actorPosX != 0 || townConfig->actorPosZ != 0))
    {
        spawnPoints.push_back(Vector2i{townConfig->actorPosX, townConfig->actorPosZ});
    }
    else if (roomConfig && !roomConfig->actorSpawns.empty())
    {
        spawnPoints.push_back(Vector2i{roomConfig->actorSpawns.front().posX, roomConfig->actorSpawns.front().posZ});
    }
    else if (campConfig && !campConfig->actorSpawns.empty())
    {
        spawnPoints.push_back(Vector2i{campConfig->actorSpawns.front().posX, campConfig->actorSpawns.front().posZ});
    }

    return loadMapByKey(mapKey, logicalId, std::move(spawnPoints));
}

bool GameWord::loadMapByKey(const std::string& mapKey, int32_t logicalId, std::vector<Vector2i> spawnPoints)
{
    if (mapKey.empty())
    {
        MG_LOG_E("GameWord::loadMapByKey: empty mapKey");
        return false;
    }

    auto directorComp = MG_GET_COMPONENT(m_director, DirectorComponent);
    if (directorComp->mapEntityId != 0)
    {
        auto oldMapEntity = ecsManager.getEntity(directorComp->mapEntityId);
        if (oldMapEntity)
        {
            oldMapEntity->destroy();
        }
    }
    directorComp->localPlayerEntityId = INVALID_ENTITY_ID;
    directorComp->cameraFollowTarget  = INVALID_ENTITY_ID;

    auto map                  = ecsManager.newEntity();
    directorComp->mapEntityId = map->getId();

    auto mapRenderComp = MG_ADD_COMPONENT(map, GameMapRenderComponent);
    auto mapComp       = MG_ADD_COMPONENT(map, GameMapComponent);
    mapComp->mapId     = logicalId > 0 ? logicalId : 0;
    mapComp->mapKey    = mapKey;
    mapComp->layerFile = std::string("mugen/map/") + mapKey + ".layer";
    mapComp->mapWidth  = 1920;
    mapComp->mapHeight = 1080;
    mapComp->scope.x      = 0;
    mapComp->scope.y      = 0;
    mapComp->scope.width  = mapComp->mapWidth;
    mapComp->scope.height = mapComp->mapHeight;
    mapComp->spawnPoints  = std::move(spawnPoints);
    if (mapComp->spawnPoints.empty())
    {
        mapComp->spawnPoints.push_back(Vector2i{mapComp->mapWidth / 2, mapComp->mapHeight / 2});
    }
    (void)mapRenderComp;

    map->notifyEntityReady();
    return true;
}

void GameWord::bindLocalPlayer(EntityId actorEntityId)
{
    auto directorComp = MG_GET_COMPONENT(m_director, DirectorComponent);
    if (!directorComp)
    {
        return;
    }
    directorComp->localPlayerEntityId = actorEntityId;
    directorComp->cameraFollowTarget  = actorEntityId;
}

bool GameWord::saveToFile(const std::string& filePath) const
{
    ByteBuffer byteBuffer(1024 * 1024 * 2);
    serialize(byteBuffer);
    byteBuffer.writeFinish();
    return io::writeDataToFile(reinterpret_cast<const char*>(byteBuffer.data()), byteBuffer.len(), filePath);
}

bool GameWord::loadFromFile(const std::string& filePath)
{
    auto data = io::getDataFromFile(filePath);
    if (data.empty())
    {
        MG_LOG_E("Failed to load file: {}", filePath);
        return false;
    }

    ByteBuffer byteBuffer(data.data(), static_cast<uint32_t>(data.size()));
    return deserialize(byteBuffer);
}

void GameWord::serialize(ByteBuffer& byteBuffer) const
{
    ecsManager.serialize(byteBuffer);
    random.serialize(byteBuffer);
    byteBuffer.writeUint32(m_director->getId());
}

bool GameWord::deserialize(ByteBuffer& byteBuffer)
{
    if (!ecsManager.deserialize(byteBuffer))
    {
        return false;
    }
    if (!random.deserialize(byteBuffer))
    {
        return false;
    }
    uint32_t directorEntityId = byteBuffer.readUint32();
    m_director                = ecsManager.getEntity(directorEntityId);
    if (m_director == nullptr)
    {
        return false;
    }

    ecsManager.postDeserializeInit();

    return true;
}

std::string GameWord::serializeToString() const
{
    ByteBuffer byteBuffer(1024 * 1024 * 2);
    serialize(byteBuffer);
    byteBuffer.writeFinish();
    return std::string(reinterpret_cast<const char*>(byteBuffer.data()), byteBuffer.len());
}

bool GameWord::deserializeFromString(const std::string& data)
{
    if (data.empty())
        return false;
    ByteBuffer byteBuffer(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(data.data())),
                          static_cast<uint32_t>(data.size()));
    return deserialize(byteBuffer);
}

NS_MG_END
