#include "AppContext.h"
#include "ui/core/FGUIPackageManager.h"
#include "audio/AudioEngine.h"
#include "ui/widgets/common/MessagePopup.h"
#include "net/client_game.pb.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace fairygui;

static AppContext* s_instance = nullptr;

AppContext& AppContext::get()
{
    AXASSERT(s_instance, "AppContext not created yet");
    return *s_instance;
}

void AppContext::create()
{
    fairygui::UIConfig::registerFont(fairygui::UIConfig::defaultFont, "fonts/Faint-DNF-Song-12px-Medium.ttf");
    fairygui::UIConfig::registerFont("Faint-DNF-Song-12px-Medium", "fonts/Faint-DNF-Song-12px-Medium.ttf");
    fairygui::UIConfig::onMusicCallback = [](const std::string& path, float volumnScale) {
        ax::AudioEngine::play2d(path, false, 1.0f);
    };

    gameui::FGUIPackageManager::getInstance().load({"UI/Common"});

    AXASSERT(!s_instance, "AppContext already created");
    s_instance = new AppContext();
}

void AppContext::destroy()
{
    gameui::FGUIPackageManager::getInstance().unload({"UI/Common"});

    // 释放全局动画资产缓存单例
    mugen::AvatarAssetCache::destroy();

    delete s_instance;
    s_instance = nullptr;
}

AppContext::AppContext()
{
    m_netClient   = std::make_unique<net::NetClient>(1);
    m_gameSession = std::make_unique<game::model::GameSessionModel>();
}

AppContext::~AppContext()
{
    if (m_connectMask)
    {
        m_connectMask->removeFromParent();
        m_connectMask = nullptr;
    }
}

void AppContext::init(ax::Scene* scene)
{
    m_viewManager = std::make_unique<gameui::ViewManager>();
    m_viewManager->init(scene);

    // 监听断线
    m_netClient->setOnDisconnect([this]() { onDisconnected(); });
    m_netClient->listenRecv([this](uint8_t cmd, uint16_t msgId, int32_t serial, const std::string_view& payload) {
        if (cmd == net::CMD_BUSINESS)
        {
            // 主动被服务器踢下线
            if (msgId == static_cast<uint16_t>(PB::Game::AccountKickedPush::Id))
            {
                m_serverHost.clear();

                std::string text = "网络连接中断";

                PB::Game::AccountKickedPush push;
                if (push.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
                {
                    AXLOGE("AccountKickedPush received: {}, reason: {}", push.message(), push.reason());
                    text = push.message();
                }

                gameui::MessagePopup::showGlobal(text, []() { ax::Director::getInstance()->end(); });
            }
        }
    }, "AppContext");
    m_netClient->setOnServiceStatusChange([this]() {
        if (!m_netClient->hasOnlineService(0))
        {
            m_serverHost.clear();
            gameui::MessagePopup::showGlobal("服务器维护中", []() { ax::Director::getInstance()->end(); });
        }
    });
}

void AppContext::update(float delta)
{
    m_viewManager->flushPendingViews();
    m_viewManager->update(delta);

    // 重连计时
    if (m_shouldReconnect && !m_connecting)
    {
        m_reconnectTimer += delta;
        if (m_reconnectTimer >= m_reconnectInterval)
        {
            m_reconnectTimer = 0.0f;
            startConnect();
        }
    }
}

void AppContext::connectToServer(const std::string& host, int port)
{
    m_serverHost        = host;
    m_serverPort        = port;
    m_shouldReconnect   = true;
    m_reconnectElapsed  = 0.0f;
    m_reconnectAttempts = 0;
    m_authenticated     = false;
    startConnect();
}

void AppContext::startConnect()
{
    m_connecting = true;
    m_reconnectAttempts++;

    showConnectMask("正在连接服务器...");

    m_netClient->setHost(m_serverHost, m_serverPort);
    m_netClient->connect([this](bool success, const std::string_view& msg) {
        if (success)
        {
            onConnected();
        }
        else
        {
            m_connecting = false;
            m_reconnectElapsed += m_reconnectInterval;

            if (m_reconnectElapsed >= m_reconnectTimeout)
            {
                // 超时，提示用户
                m_shouldReconnect = false;
                showConnectMask("网络连接失败，是否继续连接？", true);
            }
            else
            {
                showConnectMask("连接失败，正在重试...");
            }
        }
    });
}

void AppContext::onConnected()
{
    m_connecting        = false;
    m_shouldReconnect   = false;
    m_reconnectTimer    = 0.0f;
    m_reconnectElapsed  = 0.0f;
    m_reconnectAttempts = 0;
    m_authenticated     = false;

    // 向网关请求服务列表,顺便完成网关认证
    m_netClient->requestServerStatus([this](bool success, uint16_t, const std::string_view& error) {
        this->m_authenticated = success;
        hideConnectMask();
        if (!success)
        {
            AXLOGE("request server status failed: {}", error);
            gameui::MessagePopup::showGlobal("网关认证失败", []() { ax::Director::getInstance()->end(); });
        }
    }, this);
}

void AppContext::onDisconnected()
{
    if (m_gameSession)
    {
        m_gameSession->clear();
    }

    if (m_serverHost.empty())
    {
        // m_serverHost 被清空了，说明是主动断开连接，不需要重连
        return;
    }

    // 断线后启动自动重连
    m_shouldReconnect   = true;
    m_reconnectTimer    = 0.0f;
    m_reconnectElapsed  = 0.0f;
    m_reconnectAttempts = 0;
    showConnectMask("连接已断开，正在重连...");
}

void AppContext::showConnectMask(const std::string& text, bool showRetryButton)
{
    auto* groot = GRoot::getInstance();
    if (!groot)
        return;

    hideConnectMask();

    if (showRetryButton)
    {
        gameui::MessagePopup::show(text, [this]() {
            m_reconnectElapsed  = 0.0f;
            m_reconnectAttempts = 0;
            m_shouldReconnect   = true;
            m_reconnectTimer    = m_reconnectInterval;
            showConnectMask("正在连接服务器...");
        }, []() { ax::Director::getInstance()->end(); });
        return;
    }

    if (m_connectMask == nullptr)
    {
        m_connectMask = UIPackage::createObject("Common", "ConnectMaskLayer")->as<GComponent>();
        m_connectMask->setSize(groot->getWidth(), groot->getHeight());
        m_connectMask->addRelation(groot, RelationType::Size);
        m_connectMask->setSortingOrder(INT_MAX);
        groot->addChild(m_connectMask);
    }
    m_connectMask->setVisible(true);
    m_connectMask->setText(text);
}

void AppContext::hideConnectMask()
{
    if (m_connectMask)
    {
        m_connectMask->setVisible(false);
    }
}

gameui::UIManager* AppContext::uiManager() const
{
    return m_viewManager ? m_viewManager->getUIManager() : nullptr;
}
