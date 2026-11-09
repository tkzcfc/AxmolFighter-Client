#pragma once

#include "ui/core/Screen.h"
#include "ui/widgets/common/VirtualJoystick.h"
#include "mugen/sim_legacy/World.h"
#include "mugen/sim_legacy/input/ControlManagerCore.h"
#include "mugen/core/math/FixedVec3.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace PB::Types
{
class TownMovement;
}

namespace gameui
{

class TownSync;

// 进入/切换城镇参数（城际传送门传入）
struct TownBootParams
{
    int32_t townId = 41;
    // <0 表示使用 TownConfig.actorPos
    int32_t spawnX = -1;
    int32_t spawnZ = -1;
    int32_t facing = 1;
};

class TownScreen : public Screen
{
public:
    typedef Screen Super;

public:
    TownScreen();
    explicit TownScreen(TownBootParams boot);
    ~TownScreen() override;

    std::vector<std::string> getPackages() const override
    {
        // Res_GeneralIcons: MainMenu head icon (ui://37kbt1d9...)
        return {"UI/Common_Base", "UI/Res_GeneralIcons", "UI/Screen_Town"};
    }

    GComponent* onCreateContent() override
    {
        return UIPackage::createObject("Screen_Town", "TownScreen")->as<GComponent>();
    }

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
        ax::Node* node     = nullptr;
        // 世界坐标与触发半径（来自 TownConfig.portalPosX/Z 与 PortalConfig.radius）。
        mugen::Fixed posX;
        mugen::Fixed posZ;
        mugen::Fixed radius;
        // 触发边沿锁存：进入时初始化，避免出生点落在传送门上立刻触发。
        bool inside = false;
    };

    bool initTown();
    void sendEnterScene();
    void fillLocalMovement(PB::Types::TownMovement* movement) const;
    void reportLocalState(float delta);
    void applyLocalCorrection();

    void initPortals();
    void onPortalTriggered(const TownPortal& portal);
    // 判断本地玩家当前是否落在指定传送门的触发圆内。
    bool portalContains(const TownPortal& portal) const;
    // 进入场景时用当前落点初始化各传送门的触发锁存。
    void initializePortalLatches();
    // 边沿触发检测：由假变真时调用 onPortalTriggered 一次。
    void updatePortalTriggers();

    void addMessagePushReceiver();
    bool initInput();
    void onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed);
    void bindJoystick();
    void refreshControlKey(mugen::sim_legacy::ControlKey key);

    void sendDuelInvite(int64_t targetPlayerId);
    void respondDuelInvite(int32_t accept);

    void bindMainMenuUi();
    void setupMainMenuButtonIcon();
    void onClickMainMenuButton(EventContext* context);
    void onClickMaskLayer(EventContext* context);
    void openMainMenu();
    void closeMainMenu();
    void destroyMainMenu();
    void setMainMenuInteractive(bool enabled);
    void layoutMainMenu(bool offscreen);
    void bindMainMenuFeatureButtons();
    void refreshMainMenuPlayerInfo();
    void subscribeAppearanceEvents();
    void refreshLocalAppearance();
    void refreshLocalPet();

private:
    TownBootParams m_boot;
    std::vector<TownPortal> m_portals;
    mugen::sim_legacy::World m_world;
    mugen::sim_legacy::ControlManagerCore m_control;
    mugen::Entity m_localEntity;
    std::unique_ptr<TownSync> m_townSync;
    std::map<ax::EventKeyboard::KeyCode, uint32_t> m_slotMap;
    std::array<bool, static_cast<size_t>(mugen::sim_legacy::ControlKey::Count)> m_keyboardHeld{};
    VirtualJoystick m_joystick;
    ax::EventListenerKeyboard* m_keyboardListener = nullptr;
    float m_accumulator                           = 0.0f;
    bool m_initialized                            = false;
    class WorldPresenterHolder;
    std::unique_ptr<WorldPresenterHolder> m_presenter;

    bool m_enteredScene = false;
    float m_reportTimer = 0.0f;

    // 本地玩家角色半径（来自 RoleConfig），用于传送门触发圆判定。
    mugen::Fixed m_roleRadius;
    // 本次城镇生命周期内是否已触发过传送门（防止重复切城）。
    bool m_portalTriggered = false;

    // 本地移动上报：client_seq 与最近一次上报的离散状态（用于即时上报起停/朝向/状态变化）。
    uint32_t m_clientSeq  = 0;
    int32_t m_lastFacing  = 0;
    bool m_lastMoving     = false;
    std::string m_lastState;
    // 已应用的位置校正序号（拒绝更旧的校正）。
    uint32_t m_lastCorrectionSeq = 0;
    // 已确认给服务端的最近校正序号（回填到 TownMoveReport.ack_correction_seq）。
    uint32_t m_ackCorrectionSeq = 0;
    // 应用校正后强制下一帧立即上报，尽快把 ack 送回服务端。
    bool m_forceReport = false;
    // 待应用的权威位置校正（下一个逻辑帧前吸附本地玩家）。
    std::optional<mugen::FixedVec3> m_pendingCorrection;

    int64_t m_selectedPlayerId = 0;

    bool m_showDuelInvite   = false;
    int64_t m_duelInviterId = 0;
    std::string m_duelInviterName;

    GComponent* m_maskLayer      = nullptr;
    GComponent* m_mainMenuButton = nullptr;
    GComponent* m_mainMenu       = nullptr;
    bool m_mainMenuAnimating     = false;
};

}  // namespace gameui
