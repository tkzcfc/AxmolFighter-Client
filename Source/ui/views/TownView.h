#pragma once

#include "ui/core/View.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PB::Types
{
class PlayerState;
}

namespace gameui
{

// 进入/切换城镇参数（城际传送门传入）
struct TownBootParams
{
    int32_t townId = 41;
    // <0 表示使用 TownConfig.actorPos
    int32_t spawnX = -1;
    int32_t spawnZ = -1;
    int32_t facing = 1;
};

// 城镇视图。世界、移动和远程玩家渲染已拆除，保留 UI 与 town 服网络。
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
    virtual void onImGUIRender() override;

private:
    void onPrepareLoad() override;

    struct TownPortal
    {
        int32_t portalId   = 0;
        int32_t slot       = 0;
        int32_t destType   = 0;
        int32_t destTownId = 0;
        int32_t destPosX   = 0;
        int32_t destPosZ   = 0;
        int32_t destFacing = 1;
    };

    bool initTown();
    void sendEnterScene();
    void fillLocalState(PB::Types::PlayerState* state) const;
    void reportLocalState(float delta);

    void initPortals();
    void onPortalTriggered(const TownPortal& portal);

    void addMessagePushReceiver();

    void sendDuelInvite(int64_t targetPlayerId);
    void respondDuelInvite(int32_t accept);

    void showPlaceholder();

private:
    TownBootParams m_boot;
    std::vector<TownPortal> m_portals;
    ax::Label* m_placeholder = nullptr;

    bool m_enteredScene = false;
    float m_reportTimer = 0.0f;

    int64_t m_selectedPlayerId = 0;

    bool m_showDuelInvite   = false;
    int64_t m_duelInviterId = 0;
    std::string m_duelInviterName;
};

}  // namespace gameui
