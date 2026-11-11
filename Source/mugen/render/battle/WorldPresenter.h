#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/battle/AvatarPresenter.h"
#    include "mugen/render/battle/CameraDirector.h"
#    include "mugen/render/battle/DebugDraw.h"
#    include "mugen/render/battle/HurtNumber.h"
#    include "mugen/render/battle/MapPresenter.h"
#    include "mugen/render/battle/SoundPlayer.h"

#    include <string>
#    include <vector>

NS_MG_BEGIN

class WorldPresenter
{
public:
    explicit WorldPresenter(ax::Node& root);
    ~WorldPresenter();

    void loadMap(const std::string& mapKey);
    void present(sim_legacy::World& world, float interpolationAlpha, float renderDelta);
    // 消费回滚会话去重后的表现事件列表（联机路径），不做 seq 单调去重。
    void presentEvents(const std::vector<sim_legacy::PresentationEvent>& events,
                       sim_legacy::World& world,
                       float interpolationAlpha,
                       float renderDelta);
    void setFollowEntity(Entity entity);
    // 回滚纠正：见 AvatarPresenter::applyCorrection。联机模式在每次回滚后对位置被改写的角色调用。
    void applyCorrection(Entity entity, const VisualPos& shift) { m_avatars.applyCorrection(entity, shift); }
    void setPet(int32_t petId);
    void setEntityPet(Entity entity, int32_t petId);
    void clearEntityPet(Entity entity);
    void addMapNode(ax::Node& node) { m_map.addNode(node); }
    DebugDraw& debugDraw() { return m_debugDraw; }

    // Update an existing Avatar's fashion (kAsyncExclusive only).
    bool setEntityAppearance(Entity entity, const FashionAppearance& appearance);
    // Find local player Role (!isAi + playerSlot) and apply session fashion/weapon.
    bool refreshLocalPlayerAppearance(sim_legacy::World& world);

private:
    // 逐条消费表现事件；dedupeBySeq 为真时按全局单调 seq 去重（离线路径）。
    void consumeEvents(const std::vector<sim_legacy::PresentationEvent>& events, sim_legacy::World& world, bool dedupeBySeq);

    // 随镜头平移/缩放的世界容器，必须先于其余成员构造
    ax::Node& m_world;
    // 飘字与调试框的顶层，挂在 m_world 下且 z 序高于地图
    ax::Node& m_overlay;
    AvatarPresenter m_avatars;
    MapPresenter m_map;
    CameraDirector m_camera;
    SoundPlayer m_sound;
    HurtNumber m_hurtNumbers;
    DebugDraw m_debugDraw;
    Entity m_followEntity{};
    uint64_t m_lastEventSequence = 0;
};

NS_MG_END

#endif
