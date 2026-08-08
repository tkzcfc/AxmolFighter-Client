#pragma once

#include "mugen/core/ecs/Component.h"
#ifdef RUNTIME_IN_AXMOL
#    include "mugen/render/VirtualCamera.h"
#endif

NS_MG_BEGIN

// 游戏地图渲染组件（九层）
class GameMapRenderComponent : public Component
{
public:
    typedef Component Super;

#ifdef RUNTIME_IN_AXMOL
    // 视差根节点（ParallaxNode）
    ax::Node* mapRootNode = nullptr;

    // 九层（与 .layer root.children 同名）
    ax::Node* distantNode = nullptr;  // 远景
    ax::Node* middleNode  = nullptr;  // 中景
    ax::Node* nearbyNode  = nullptr;  // 近景
    ax::Node* groundNode  = nullptr;  // 地面
    ax::Node* regionNode  = nullptr;  // 区域（可行走范围等逻辑）
    ax::Node* triggerNode = nullptr;  // 触发区
    ax::Node* entityNode  = nullptr;  // 实体（角色/NPC/传送门挂点）
    ax::Node* caseNode    = nullptr;  // 遮罩/前景物件
    ax::Node* lightNode   = nullptr;  // 灯光/特效

    std::unique_ptr<VirtualCamera> camera;

    // debug 挂在 entity 下
    ax::DrawNode* groundDebugDrawNode = nullptr;  // 地面碰撞调试
    ax::DrawNode* actorDebugDrawNode  = nullptr;  // 角色框体调试
#endif

public:
    GameMapRenderComponent() {}
    virtual ~GameMapRenderComponent() {}
};

NS_MG_END
