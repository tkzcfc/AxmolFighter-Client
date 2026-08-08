#pragma once

#include "mugen/core/math/Vec2.h"

NS_MG_BEGIN

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

class MapConfig : public Object
{
public:
    typedef Object Super;

public:
    MapConfig() = default;

    virtual ~MapConfig() {}

public:
    // 配置源文件路径(不参与序列化,在配置加载时由Config自动赋值)
    std::string sourcePath;

    std::string layerFile;

    // 地图中预设的出生点列表
    std::vector<Vector2i> spawnPoints;

    int32_t mapWidth  = 0;
    int32_t mapHeight = 0;
    MapScope scope;

    std::string walkSound;
    std::string dashSound;
    std::string bgmSound;
    std::string name;

    MG_DEFINE_SERIALIZABLE(layerFile, spawnPoints, mapWidth, mapHeight, scope, walkSound, dashSound, bgmSound, name);
};

NS_MG_END
