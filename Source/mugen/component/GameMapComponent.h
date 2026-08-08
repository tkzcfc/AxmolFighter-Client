#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/Config.h"

NS_MG_BEGIN

// 游戏地图组件
class GameMapComponent : public Component
{
public:
    typedef Component Super;

public:
    GameMapComponent() {}
    virtual ~GameMapComponent() {}

    int32_t mapId              = 0;
    int32_t mapDataId          = 0;  // MapDataConfig id（视差/BGM）
    const MapConfig* mapConfig = nullptr;

    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl, mapId, mapDataId)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const
    {
        byteBuffer.writeString(mapConfig ? mapConfig->sourcePath : "");
    }

    bool deserializeCustomImpl(ByteBuffer& byteBuffer)
    {
        std::string mapConfigSourcePath = byteBuffer.readString();
        if (mapConfigSourcePath.empty())
        {
            mapConfig = nullptr;
        }
        else
        {
            mapConfig = Config::getInstance()->getMapConfig(mapConfigSourcePath);
            if (!mapConfig)
            {
                MG_LOG_E("Failed to load map config: {}", mapConfigSourcePath);
                return false;
            }
        }
        return true;
    }
};

NS_MG_END
