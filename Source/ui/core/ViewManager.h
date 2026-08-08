#pragma once

#include "axmol.h"
#include "FairyGUI.h"

namespace gameui
{
using namespace fairygui;

class View;
class UIManager;

class ViewManager
{
public:
    ViewManager();
    ~ViewManager();

    void init(ax::Scene* scene);
    void update(float dt);

    // 排队切换；真正销毁旧 View / 挂载新 View 在 flushPendingViews 时执行。
    // 返回指向 pending View 的指针（已构造，尚未 _create / 入栈）。
    // flush 前 getCurrentView() 仍返回当前栈顶。
    template <typename T, typename... Args>
    T* switchView(Args&&... args)
    {
        auto view = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr    = view.get();
        _queuePending(PendingViewAction::Switch, std::move(view));
        return ptr;
    }

    template <typename T, typename... Args>
    T* pushView(Args&&... args)
    {
        auto view = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr    = view.get();
        _queuePending(PendingViewAction::Push, std::move(view));
        return ptr;
    }

    bool popView();

    // 在安全点应用排队中的 switch/push
    void flushPendingViews();

    View* getCurrentView() const { return m_viewStack.empty() ? nullptr : m_viewStack.back().get(); }
    UIManager* getUIManager() const { return m_uiManager.get(); }

private:
    enum class PendingViewAction : uint8_t
    {
        None,
        Switch,
        Push,
    };

    void _queuePending(PendingViewAction action, std::unique_ptr<View> newView);
    void _switchView(std::unique_ptr<View> newView);
    void _pushView(std::unique_ptr<View> newView);
    void _destroyView(std::unique_ptr<View>& view);
    void _showView(View* view, bool visible);

    GRoot* m_groot          = nullptr;
    GComponent* m_viewLayer = nullptr;
    GComponent* m_uiLayer   = nullptr;
    std::vector<std::unique_ptr<View>> m_viewStack;
    std::unique_ptr<UIManager> m_uiManager;

    PendingViewAction m_pendingAction = PendingViewAction::None;
    std::unique_ptr<View> m_pendingView;
};

}  // namespace gameui
