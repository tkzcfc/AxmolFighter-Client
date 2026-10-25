#pragma once

#include <memory>
#include <string>
#include "net/NetClient.h"
#include "model/GameSessionModel.h"
#include "resource/ResourceLoader.h"
#include "ui/core/UIManager.h"
#include "ui/core/ScreenManager.h"
#include "axmol.h"
#include "FairyGUI.h"

namespace game
{
class EventDispatcher;
}

namespace gameui
{
class FeatureProgressSync;
class WeaponLocalState;
class SkillLocalState;
class PetLocalState;
class PartnerLocalState;
}

class AppContext
{
public:
    static AppContext& get();
    static void create();
    static void destroy();

    void init(ax::Scene* scene);
    void update(float delta);

    net::NetClient* netClient() const { return m_netClient.get(); }
    game::model::GameSessionModel* gameSession() const { return m_gameSession.get(); }
    gameui::ScreenManager* screenManager() const { return m_screenManager.get(); }
    gameui::UIManager* uiManager() const;

    game::EventDispatcher* eventDispatcher() const { return m_eventDispatcher.get(); }
    gameui::FeatureProgressSync* featureProgressSync();
    gameui::WeaponLocalState* weaponLocalState() const { return m_weaponLocalState.get(); }
    gameui::SkillLocalState* skillLocalState() const { return m_skillLocalState.get(); }
    gameui::PetLocalState* petLocalState() const { return m_petLocalState.get(); }
    gameui::PartnerLocalState* partnerLocalState() const { return m_partnerLocalState.get(); }

    // 生命周期与 AppContext 相同的常驻资源（配置表等，不一定都在 UI/Common_Base 里）；
    // 由 LaunchScreen 通过 setResourceLoader() 共同持有并启动加载，后续也可以继续往里面加东西。
    std::shared_ptr<gameres::ResourceLoader> globalLoader() const { return m_globalLoader; }

    /// 连接到服务器
    void connectToServer(const std::string& host, int port);
    /// 连接到默认服务器
    void connectToDefaultServer();

private:
    AppContext();
    ~AppContext();

    AppContext(const AppContext&)            = delete;
    AppContext& operator=(const AppContext&) = delete;

    void startConnect();
    void onConnected();
    void onDisconnected();
    void showConnectMask(const std::string& text, bool showRetryButton = false);
    void hideConnectMask();

    std::unique_ptr<net::NetClient> m_netClient;
    std::unique_ptr<game::model::GameSessionModel> m_gameSession;
    // EventDispatcher must outlive ScreenManager / local states: their onExit /
    // destructors call unsubscribeAll. Members destroy in reverse declaration order.
    std::unique_ptr<game::EventDispatcher> m_eventDispatcher;
    std::unique_ptr<gameui::FeatureProgressSync> m_featureProgressSync;
    std::unique_ptr<gameui::WeaponLocalState> m_weaponLocalState;
    std::unique_ptr<gameui::SkillLocalState> m_skillLocalState;
    std::unique_ptr<gameui::PetLocalState> m_petLocalState;
    std::unique_ptr<gameui::PartnerLocalState> m_partnerLocalState;
    std::unique_ptr<gameui::ScreenManager> m_screenManager;
    std::shared_ptr<gameres::ResourceLoader> m_globalLoader;

    // 连接遮罩
    fairygui::GComponent* m_connectMask = nullptr;

    // 重连状态
    std::string m_serverHost;
    int m_serverPort          = 0;
    float m_reconnectTimer    = 0.0f;
    float m_reconnectInterval = 3.0f;   // 每次重试间隔（秒）
    float m_reconnectElapsed  = 0.0f;   // 累计重连时间
    float m_reconnectTimeout  = 15.0f;  // 超过此时间提示用户
    int m_reconnectAttempts   = 0;
    bool m_connecting         = false;
    bool m_shouldReconnect    = false;
    // 是否认证成功
    bool m_authenticated = false;
};
