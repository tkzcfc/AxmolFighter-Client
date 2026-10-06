#pragma once

#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/math/Random.h"
#include "mugen/core/math/Vec2.h"

#include <string>

#if RUNTIME_IN_AXMOL
#    include <axmol.h>
#endif  // RUNTIME_IN_AXMOL

NS_MG_BEGIN

class TownConfig;
class RoomConfig;
class CampConfig;

// 游戏世界运行模式
enum class GameWordMode : int8_t
{
    // 战斗模式：完整战斗逻辑
    kBattle = 0,
    // 城镇模式：不自动刷角色，由外部驱动实体创建
    kTown = 1,
};

// mapId（Town/Room/Camp 共用一个 id 空间）解析到的配置和 mapKey；只读配置，不做任何加载。
// loadMap 和预加载（TownView/GameView::onPrepareLoad）都用它来保证两边解析规则一致。
struct MapResolveResult
{
    std::string mapKey;
    const TownConfig* townConfig = nullptr;
    const RoomConfig* roomConfig = nullptr;
    const CampConfig* campConfig = nullptr;

    bool isValid() const { return !mapKey.empty(); }
    // mugen/map/<mapKey>.layer
    std::string layerFile() const { return "mugen/map/" + mapKey + ".layer"; }
};

class GameWord
{
public:
    GameWord();
    ~GameWord();

#ifdef RUNTIME_IN_AXMOL
    bool init(ax::Node* node, uint64_t randomSeed);
#else
    bool init(uint64_t randomSeed);
#endif

    void update(float dt);

    // 按 Town/Room/Camp 共用的 mapId 解析出 mapKey 和对应配置；不读文件，不创建实体。
    static MapResolveResult resolveMap(int32_t mapId);

    bool loadMap(int32_t mapId);

    // 按 mapKey 加载（mugen/map/<key>.layer）
    // logicalId 用于区分同一 mapKey 的不同逻辑地图（如房间/城镇），spawnPoints 用于指定玩家出生点
    bool loadMapByKey(const std::string& mapKey, int32_t logicalId = 0, std::vector<Vector2i> spawnPoints = {});

    // 绑定本机操控角色
    void bindLocalPlayer(EntityId actorEntityId);

    ECSManager ecsManager;

    Random random;

#ifdef RUNTIME_IN_AXMOL
    MG_SYNTHESIZE_READONLY(ax::Node*, m_wordRootNode, WordRootNode);
#endif
    MG_SYNTHESIZE_READONLY(Entity*, m_director, Director);

    MG_SYNTHESIZE(GameWordMode, m_mode, Mode);

    bool saveToFile(const std::string& filePath) const;

    bool loadFromFile(const std::string& filePath);

    void serialize(ByteBuffer& byteBuffer) const;

    bool deserialize(ByteBuffer& byteBuffer);
};

NS_MG_END
