#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include "FairyGUI.h"
#include "ViewManager.h"
#include "net/NetErr.h"
#include "net/NetAgent.h"

namespace gameui
{
using namespace fairygui;

class View : public net::NetAgent
{
public:
    View();
    virtual ~View();

    // 返回依赖的 FairyGUI 包资源路径。
    // 例如 { "UI/Common", "UI/Login" }
    virtual std::vector<std::string> getPackages() const { return {}; }

    virtual GComponent* onCreateContent() { return nullptr; }

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onUpdate(float /*dt*/) {}
    // ImGui 渲染回调
    virtual void onImGUIRender() {}

    GComponent* getContent() const { return m_root; }
    ViewManager* getViewManager() const { return m_viewManager; }

    void addClickListener(UIEventDispatcher* dispatcher, const std::function<void(EventContext*)>& callback);

    template <typename T>
    T* getChild(const std::string& name) const
    {
        GObject* obj = m_root->getChild(name);
        AXASSERT(obj, ("UI child not found: " + name).c_str());
        T* result = obj->as<T>();
        AXASSERT(result, ("UI child type mismatch: " + name).c_str());
        return result;
    }

private:
    friend class ViewManager;

    void _create();
    void _destroy();

    GComponent* m_root         = nullptr;
    ViewManager* m_viewManager = nullptr;
};

}  // namespace gameui
