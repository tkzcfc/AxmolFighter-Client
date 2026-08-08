#pragma once

#include <memory>
#include <string>
#include "net/NetClient.h"
#include "model/GameSessionModel.h"
#include "ui/core/UIManager.h"
#include "ui/core/ViewManager.h"
#include "axmol.h"
#include "FairyGUI.h"

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
    gameui::ViewManager* viewManager() const { return m_viewManager.get(); }
    gameui::UIManager* uiManager() const;

    /// 连接到服务器
    void connectToServer(const std::string& host, int port);

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
    std::unique_ptr<gameui::ViewManager> m_viewManager;

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
