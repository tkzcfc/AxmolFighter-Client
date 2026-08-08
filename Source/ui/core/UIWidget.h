#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "FairyGUI.h"
#include "UITypes.h"
#include "UIManager.h"
#include "net/NetErr.h"
#include "net/NetAgent.h"

namespace gameui
{
using namespace fairygui;

class View;

class UIWidget : public std::enable_shared_from_this<UIWidget>, public net::NetAgent
{
public:
    UIWidget();
    virtual ~UIWidget();

    void close();

    bool isVisible() const;
    bool isFullscreen() const { return m_options.fullscreen; }
    bool hasBackground() const { return m_options.hasBackground; }
    UIState getState() const { return m_state; }

    GComponent* getRoot() const { return m_root; }
    GComponent* getContent() const { return m_content; }
    GGraph* getBackground() const { return m_background; }
    UIManager* getUIManager() const { return m_manager; }

    void setBackgroundVisible(bool visible);

    void addClickListener(UIEventDispatcher* dispatcher, const std::function<void(EventContext*)>& callback);

    template <typename T>
    T* getChild(const std::string& name) const
    {
        GComponent* container = m_content ? m_content : m_root;
        AXASSERT(container, "getChild called before UI was created");
        GObject* obj = container->getChild(name);
        AXASSERT(obj, ("UI child not found: " + name).c_str());
        T* result = obj->as<T>();
        AXASSERT(result, ("UI child type mismatch: " + name).c_str());
        return result;
    }

    // 创建一个 GComponent 并将他居中显示和关联
    GComponent* createCenteredComponent(const std::string& pkgName, const std::string& resName);

protected:
    // 返回此控件依赖的 FairyGUI 包资源路径。
    // 例如 { "UI/Common", "UI/Popup" }
    // UIManager 会在创建控件时加载这些包，在销毁控件时卸载这些包。
    virtual std::vector<std::string> getPackages() const { return {}; }

    // 子类自行创建 UI
    virtual GComponent* onCreateContent() { return nullptr; }

    // 在创建时调用
    virtual void onCreate() {}

    // 在界面打开动画执行完毕后调用
    virtual void onShow() {}

    // 在界面关闭动画执行完毕后调用
    virtual void onHide() {}

    // 在销毁时调用
    virtual void onDestroy() {}

    // 更新函数
    virtual void onUpdate(float /*dt*/) {}

    // 在销毁时调用,子类可以停止自定义的显示/隐藏动画
    virtual void onStopAnimations() {}

    // 播放显示动画，done 回调必须且只能被调用一次，调用时机由子类决定
    virtual void doShowAnimation(std::function<void()> done);

    // 播放隐藏动画，done 回调必须且只能被调用一次，调用时机由子类决定
    virtual void doHideAnimation(std::function<void()> done);

    UIWidgetOptions m_options;

private:
    friend class UIManager;

    void _create();
    void _destroy();
    void _doShow();
    void _doHide();

    GComponent* m_root            = nullptr;
    GComponent* m_content         = nullptr;
    GGraph* m_background          = nullptr;
    UIState m_state               = UIState::None;
    UIManager* m_manager          = nullptr;
    View* m_ownerView             = nullptr;
    UIWidgetLifecycle m_lifecycle = UIWidgetLifecycle::WithOwner;
    uint32_t m_zorder             = 0;
    bool m_stackVisible           = true;
};

}  // namespace gameui
