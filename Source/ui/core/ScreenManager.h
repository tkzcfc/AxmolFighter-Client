#pragma once

#include "axmol.h"
#include "FairyGUI.h"

namespace gameui
{
using namespace fairygui;

class Screen;
class UIManager;

class ScreenManager
{
public:
    ScreenManager();
    ~ScreenManager();

    void init(ax::Scene* scene);
    void update(float dt);

    // 排队切换；真正销毁旧 Screen / 挂载新 Screen 在 flushPendingScreens 时执行。
    // 返回指向 pending Screen 的指针（已构造，尚未 _create / 入栈）。
    // flush 前 getCurrentScreen() 仍返回当前栈顶。
    template <typename T, typename... Args>
    T* switchScreen(Args&&... args)
    {
        if (isLoading())
        {
            AXLOGW("ScreenManager: ignore switchScreen while current Screen is loading");
            return nullptr;
        }

        auto screen = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr    = screen.get();
        _queuePending(PendingScreenAction::Switch, std::move(screen));
        return ptr;
    }

    // 压入新的 Screen 到栈顶
    template <typename T, typename... Args>
    T* pushScreen(Args&&... args)
    {
        if (isLoading())
        {
            AXLOGW("ScreenManager: ignore pushScreen while current Screen is loading");
            return nullptr;
        }

        auto screen = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr    = screen.get();
        _queuePending(PendingScreenAction::Push, std::move(screen));
        return ptr;
    }

    // 弹出当前栈顶 Screen
    bool popScreen();

    // 在安全点应用排队中的 switch/push
    void flushPendingScreens();

    // 获取当前栈顶 Screen；如果栈为空，返回 nullptr。
    Screen* getCurrentScreen() const { return m_screenStack.empty() ? nullptr : m_screenStack.back().get(); }

    // 当前Screen是否正在加载资源（即处于 ScreenState::Loading 状态）
    bool isLoading() const;

    // 获取 UIManager 实例，用于管理 UIWidget
    UIManager* getUIManager() const { return m_uiManager.get(); }

private:
    enum class PendingScreenAction : uint8_t
    {
        None,
        Switch,
        Push,
    };

    void _queuePending(PendingScreenAction action, std::unique_ptr<Screen> newScreen);
    void _switchScreen(std::unique_ptr<Screen> newScreen);
    void _pushScreen(std::unique_ptr<Screen> newScreen);
    void _destroyScreen(std::unique_ptr<Screen>& screen);
    void _showScreen(Screen* screen, bool visible);

    GRoot* m_groot          = nullptr;
    GComponent* m_screenLayer = nullptr;
    GComponent* m_uiLayer   = nullptr;
    std::vector<std::unique_ptr<Screen>> m_screenStack;
    std::unique_ptr<UIManager> m_uiManager;

    PendingScreenAction m_pendingAction = PendingScreenAction::None;
    std::unique_ptr<Screen> m_pendingScreen;

    // _switchScreen 之后延后到新 Screen 进入 Active 才清理共享 Spine 数据（见 ScreenManager.cpp 的说明）
    bool m_purgeSpineCachePending = false;
};

}  // namespace gameui
