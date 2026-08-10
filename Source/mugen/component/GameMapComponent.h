#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/math/Vec2.h"

NS_MG_BEGIN

// 可行走/物理范围（由 .layer moveRange 汇总）
class MapScope : public Object
{
public:
    typedef Object Super;

public:
    int32_t x      = 0;
    int32_t y      = 0;
    int32_t width  = 0;
    int32_t height = 0;
    MG_DEFINE_SERIALIZABLE(x, y, width, height)
};

// 游戏地图组件（原 MapConfig 运行时字段内联于此）
class GameMapComponent : public Component
{
public:
    typedef Component Super;

public:
    GameMapComponent() {}
    virtual ~GameMapComponent() {}

    // 逻辑 id（town/room 等，刷怪查表用）
    int32_t mapId = 0;

    // 场景 key → mugen/map/<mapKey>.layer
    std::string mapKey;
    std::string layerFile;

    int32_t mapWidth  = 0;
    int32_t mapHeight = 0;
    MapScope scope;

    // .layer Root/meta.soundId
    int32_t soundId = 0;

    std::vector<Vector2i> spawnPoints;

    MG_DEFINE_SERIALIZABLE(mapId, mapKey, layerFile, mapWidth, mapHeight, scope, soundId, spawnPoints)
};

NS_MG_END
