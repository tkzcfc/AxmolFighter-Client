#pragma once

#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/math/Random.h"
#include "mugen/core/math/Vec2.h"

#if RUNTIME_IN_AXMOL
#    include <axmol.h>
#endif  // RUNTIME_IN_AXMOL

NS_MG_BEGIN

// 游戏世界运行模式
enum class GameWordMode : int8_t
{
    // 战斗模式：完整战斗逻辑
    kBattle = 0,
    // 城镇模式：不自动刷角色，由外部驱动实体创建
    kTown = 1,
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
