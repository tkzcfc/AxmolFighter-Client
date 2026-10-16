#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/render/Avatar.h"
#    include "mugen/render/LayerRuntimeLoader.h"
#    include "mugen/render/VirtualCamera.h"
#    include "mugen/render/spine/MgSkeletonAnimation.h"

#    include <map>
#    include <random>

NS_MG_BEGIN

class AvatarPresenter
{
public:
    explicit AvatarPresenter(ax::Node& root) : m_root(root) {}

    void present(sim::World& world, float alpha, float renderDelta);
    void clear();

private:
    // 单个实体的渲染状态：外观只在首次出现时解析一次。
    struct AvatarState
    {
        // 绑定动作文件的实体由 Avatar 按逻辑动作时间摆姿。
        Avatar* avatar = nullptr;
        // 逻辑侧未绑定动作文件的实体（传送门）直接按动作名播放 Spine 动画。
        MgSkeletonAnimation* skeleton = nullptr;
        // 外观已解析；无 Spine 资源的实体两者均为空且不再重试。
        bool resolved = false;
        // 配置的 Spine 相对偏移（x 随水平朝向、y 向上、z 随纵深朝向）。
        std::vector<int32_t> spineOffset;
        sim::Transform previous;
        bool hasPrevious = false;
        std::string motionName;
        std::string entryId;
    };

    // 返回实体当前的显示节点；未创建时为空。
    static ax::Node* displayNode(const AvatarState& state);
    // 返回配置层级对应的绘制层，首次使用时按层级值作为 z 序创建。
    ax::Node& tierLayer(int32_t tier);

    ax::Node& m_root;
    std::map<uint32_t, AvatarState> m_avatars;
    // 配置层级 → 绘制层；层间按层级值排序，层内按纵深排序。
    std::map<int32_t, ax::Node*> m_tierLayers;
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
    bool enabled() const { return m_enabled; }

private:
    ax::DrawNode* m_node = nullptr;
    bool m_enabled       = false;
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
