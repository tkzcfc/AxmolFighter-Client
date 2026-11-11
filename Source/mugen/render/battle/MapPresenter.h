#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/VirtualCamera.h"

#    include <memory>
#    include <string>

NS_MG_BEGIN

class MapPresenter
{
public:
    // world 是随镜头平移/缩放的容器：地图 ParallaxNode 与顶层 overlay 都挂在它下面
    explicit MapPresenter(ax::Node& world) : m_world(world) {}

    void load(const std::string& mapKey);
    void update(float delta);
    void addNode(ax::Node& node) { m_world.addChild(&node); }
    VirtualCamera* camera() const { return m_camera.get(); }
    // 地图 entity 层；load 之后始终有效。
    ax::Node& entityLayer() const { return *m_entityLayer; }
    // 按层名取地图子层；不存在时返回空。
    ax::Node* layer(const std::string& name) const;

private:
    ax::Node& m_world;
    ax::ParallaxNode* m_map = nullptr;
    ax::Node* m_entityLayer = nullptr;
    std::unique_ptr<VirtualCamera> m_camera;
    std::string m_mapKey;
};

NS_MG_END

#endif
