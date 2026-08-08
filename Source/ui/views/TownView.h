#pragma once

#include "ui/core/View.h"
#include "mugen/GameWord.h"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace PB::Types
{
class PlayerState;
}

namespace ax
{
class Node;
}

namespace gameui
{

// 进入/切换城镇参数（城际传送门传入）
struct TownBootParams
{
    int32_t townId = 41;
    // <0 表示使用 TownConfig.actorPos
    int32_t spawnX  = -1;
    int32_t spawnZ  = -1;
    int32_t facing  = 1;
};

// 城镇视图：本地玩家使用完整 Mugen 逻辑移动，通过 town 服状态同步
// 展示同场景其他玩家（远程实体只做渲染插值，不参与逻辑模拟）。
class TownView : public View
{
public:
    typedef View Super;

public:
    TownView();
    explicit TownView(TownBootParams boot);

    virtual void onEnter() override;
    virtual void onExit() override;
    virtual void onUpdate(float delta) override;

    // Keyboard
    void onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event);

private:
    // 远程玩家同步数据
    struct RemotePlayer
    {
        mugen::EntityId entityId = mugen::INVALID_ENTITY_ID;
        std::string name;
        int32_t classId = 0;
        int32_t hp      = 0;

        // 插值：当前渲染位置 / 追赶目标 / 最近网络权威位置
        float curX = 0.0f, curY = 0.0f, curZ = 0.0f;
        float targetX = 0.0f, targetY = 0.0f, targetZ = 0.0f;
        float authX = 0.0f, authY = 0.0f;
        // 最近移动方向（单位向量），无移动输入时清零
        float dirX = 0.0f, dirY = 0.0f;
        // 角色移动速度（像素/秒），用于恒速追赶目标位置
        float moveSpeed = 850.0f;
        // 当前正在播放的动作
        std::string motion;
        // 暂缓应用的动作。只有"切回 idle"会被暂缓（等位置追上目标再切），
        // 其余动作（起步 walk、技能等）都立即应用
        std::string pendingMotion;
        // 该职业的 idle / walk 动作名（BehaviorTemplate / Spine 默认）
        std::string idleMotion;
        std::string walkMotion;
        // 网络侧是否仍在主动移动（PlayerState.moving）
        bool moving = false;
        // 当前视觉朝向（与实际追赶方向对齐）
        int32_t facing = 0;
    };

    // 城镇传送门运行时数据
    struct TownPortal
    {
        int32_t portalId   = 0;
        int32_t slot       = 0;
        int32_t destType   = 0;
        int32_t destTownId = 0;
        int32_t destPosX   = 0;
        int32_t destPosZ   = 0;
        int32_t destFacing = 1;
        float posX         = 0.0f;
        float posY         = 0.0f;
        float radius       = 0.0f;
        bool playerInside  = false;
        ax::Node* visual   = nullptr;
    };

    bool initGameWord();
    bool createLocalPlayer();
    void sendEnterScene();
    void fillLocalState(PB::Types::PlayerState* state) const;
    void reportLocalState(float delta);

    // 传送门
    void initPortals();
    void updatePortals();
    void onPortalTriggered(const TownPortal& portal);
    static ax::Node* findChildByNameRecursive(ax::Node* root, const std::string& name);

    // 添加消息推送监听
    void addMessagePushReceiver();
    void addOrUpdateRemotePlayer(const PB::Types::PlayerState& state, bool snap);
    void removeRemotePlayer(int64_t playerId);
    void applyRemoteState(RemotePlayer& remote, const PB::Types::PlayerState& state, bool snap);
    void applyPendingMotion(RemotePlayer& remote, mugen::Entity* entity);
    void updateRemoteInterpolation(float delta);

    virtual void onImGUIRender() override;

    mugen::Entity* getLocalPlayer() const;

    // 决斗
    void sendDuelInvite(int64_t targetPlayerId);
    void respondDuelInvite(int32_t accept);

    // 单人联网战斗
    void requestSoloBattle();

private:
    TownBootParams m_boot;
    std::map<ax::EventKeyboard::KeyCode, uint32_t> m_slotMap;
    std::unique_ptr<mugen::GameWord> m_gameWord   = nullptr;
    ax::EventListenerKeyboard* m_keyboardListener = nullptr;

    // player_id -> 远程玩家数据
    std::unordered_map<int64_t, RemotePlayer> m_remotePlayers;

    // 城镇传送门
    std::vector<TownPortal> m_portals;

    bool m_enteredScene = false;

    // 状态上报节流
    float m_reportTimer = 0.0f;
    float m_lastSentX   = 0.0f;
    float m_lastSentY   = 0.0f;
    float m_lastSentZ   = 0.0f;
    std::string m_lastSentMotion;
    int32_t m_lastSentFacing = -1;
    bool m_lastSentMoving    = false;
    float m_lastSentVelX     = 0.0f;
    float m_lastSentVelY     = 0.0f;

    // ImGui 玩家列表选中项
    int64_t m_selectedPlayerId = 0;

    // 决斗邀请弹窗状态
    bool m_showDuelInvite   = false;
    int64_t m_duelInviterId = 0;
    std::string m_duelInviterName;
};

}  // namespace gameui
