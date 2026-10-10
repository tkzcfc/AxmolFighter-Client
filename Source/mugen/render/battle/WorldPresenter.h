#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/render/Avatar.h"
#    include "mugen/render/LayerRuntimeLoader.h"
#    include "mugen/render/VirtualCamera.h"

#    include <map>
#    include <random>

NS_MG_BEGIN

class AvatarPresenter
{
public:
    explicit AvatarPresenter(ax::Node& root) : m_root(root) {}

    void present(sim::World& world, float alpha);
    void clear();

private:
    struct AvatarState
    {
        Avatar* avatar = nullptr;
        sim::Transform previous;
        bool hasPrevious = false;
        std::string motionName;
        std::string entryId;
    };

    ax::Node& m_root;
    std::map<uint32_t, AvatarState> m_avatars;
};

class MapPresenter
{
public:
    // root 挂地图节点；worldLayer 挂角色等世界内容，镜头同时平移两者
    MapPresenter(ax::Node& root, ax::Node& worldLayer) : m_root(root), m_worldLayer(worldLayer) {}

    void load(const std::string& mapKey);
    void update(float delta);
    void addNode(ax::Node& node) { m_worldLayer.addChild(&node); }
    VirtualCamera* camera() const { return m_camera.get(); }

private:
    ax::Node& m_root;
    // 世界坐标内容层（角色、伤害数字、调试绘制、地图挂点）
    ax::Node& m_worldLayer;
    ax::ParallaxNode* m_map = nullptr;
    std::unique_ptr<VirtualCamera> m_camera;
    std::string m_mapKey;
};

class CameraDirector
{
public:
    explicit CameraDirector(MapPresenter& map) : m_map(map) {}

    void follow(sim::World& world, Entity entity);
    void consume(const sim::PresentationEvent& event);
    void update(float delta);

private:
    MapPresenter& m_map;
    float m_displaySlowScale = 1.0f;
};

class SoundPlayer
{
public:
    explicit SoundPlayer(uint32_t seed = 0x51A7u) : m_random(seed) {}

    void consume(const sim::PresentationEvent& event);

private:
    std::minstd_rand m_random;
};

class HurtNumber
{
public:
    explicit HurtNumber(ax::Node& root) : m_root(root) {}
    void consume(const sim::PresentationEvent& event, sim::World& world);

private:
    ax::Node& m_root;
};

class DebugDraw
{
public:
    explicit DebugDraw(ax::Node& root);
    void present(sim::World& world);
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    ax::DrawNode* m_node = nullptr;
    bool m_enabled = false;
};

class WorldPresenter
{
public:
    explicit WorldPresenter(ax::Node& root);

    void loadMap(const std::string& mapKey);
    void present(sim::World& world, float interpolationAlpha, float renderDelta);
    void setFollowEntity(Entity entity) { m_followEntity = entity; }
    void addMapNode(ax::Node& node) { m_map.addNode(node); }
    DebugDraw& debugDraw() { return m_debugDraw; }

private:
    // 世界内容层，必须先于其余成员构造
    ax::Node& m_worldLayer;
    AvatarPresenter m_avatars;
    MapPresenter m_map;
    CameraDirector m_camera;
    SoundPlayer m_sound;
    HurtNumber m_hurtNumbers;
    DebugDraw m_debugDraw;
    Entity m_followEntity;
    uint64_t m_lastEventSequence = 0;
};

NS_MG_END

#endif
