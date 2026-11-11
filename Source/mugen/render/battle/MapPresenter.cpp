#include "MapPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/LayerLoader.h"
#    include "mugen/render/LayerRuntimeLoader.h"

NS_MG_BEGIN

void MapPresenter::load(const std::string& mapKey)
{
    if (m_mapKey == mapKey)
        return;
    if (m_map)
        m_map->removeFromParent();
    m_mapKey               = mapKey;
    const std::string path = mapKey.ends_with(".layer") ? mapKey : "/mugen/map/" + mapKey + ".layer";
    m_map                  = LayerRuntimeLoader::loadNode(path);
    m_world.addChild(m_map, 0);
    m_entityLayer = LayerRuntimeLoader::findLayer(*m_map, "entity");
    if (!m_entityLayer)
    {
        MG_LOG_E("MapPresenter: map '{}' has no entity layer", path);
        MG_ASSERT(false && "Map is missing the entity layer");
    }
    m_camera              = VirtualCamera::create();
    const IntVec2 mapSize = LayerLoader::load(path).size;
    if (mapSize.x > 0 && mapSize.y > 0)
        m_camera->setRegion({static_cast<float>(mapSize.x), static_cast<float>(mapSize.y)});
    else
        MG_LOG_E("MapPresenter: map '{}' has no size, camera cannot scroll", path);
    m_camera->setCall([world = &m_world](float x, float y, float zoom) {
        world->setPosition(x, y);
        world->setScale(zoom);
    });
}

void MapPresenter::update(float delta)
{
    if (m_camera)
        m_camera->doUpdate(delta);
}

ax::Node* MapPresenter::layer(const std::string& name) const
{
    return m_map ? LayerRuntimeLoader::findLayer(*m_map, name) : nullptr;
}

NS_MG_END

#endif
