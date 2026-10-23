#pragma once

#include "ui/core/Screen.h"
#include "mugen/sim/World.h"
#include "mugen/sim/input/ControlManagerCore.h"

#include <cstdint>
#include <map>
#include <memory>
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
        return {"UI/Res_GeneralIcons", "UI/Screen_Town"};
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
    };

    bool initTown();
    void sendEnterScene();
    void fillLocalState(PB::Types::PlayerState* state) const;
    void reportLocalState(float delta);

    void initPortals();
    void onPortalTriggered(const TownPortal& portal);

    void addMessagePushReceiver();
    bool initInput();
    void onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed);
    void addOrUpdateRemotePlayer(const PB::Types::PlayerState& state);
    void removeRemotePlayer(int64_t playerId);
    void queuePuppetUpdate(mugen::Entity entity, const PB::Types::PlayerState& state);
    mugen::Entity localEntity() const;

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

private:
    TownBootParams m_boot;
    std::vector<TownPortal> m_portals;
    mugen::sim::World m_world;
    mugen::sim::ControlManagerCore m_control;
    mugen::Entity m_localEntity;
    std::map<int64_t, mugen::Entity> m_remotePlayers;
    std::vector<mugen::sim::PuppetUpdate> m_pendingPuppetUpdates;
    std::map<ax::EventKeyboard::KeyCode, uint32_t> m_slotMap;
    ax::EventListenerKeyboard* m_keyboardListener = nullptr;
    float m_accumulator                           = 0.0f;
    bool m_initialized                            = false;
    class WorldPresenterHolder;
    std::unique_ptr<WorldPresenterHolder> m_presenter;

    bool m_enteredScene = false;
    float m_reportTimer = 0.0f;

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
