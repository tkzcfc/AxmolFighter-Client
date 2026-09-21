#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>
#include "FairyGUI.h"
#include "GLoader3D.h"
#include "ResourceLoadOperation.h"
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

    // 创建资源加载逻辑；如果返回 nullptr，则表示没有资源加载逻辑，直接创建正式内容。
    virtual std::unique_ptr<IResourceLoadOperation> createResourceLoadOperation() { return nullptr; }

    // 返回依赖的 FairyGUI 包资源路径。
    // 例如 { "UI/Common", "UI/Login" }
    virtual std::vector<std::string> getPackages() const { return {}; }

    // 创建正式内容；如果返回 nullptr，则创建一个空的 GComponent。
    virtual GComponent* onCreateContent() { return nullptr; }

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onUpdate(float /*dt*/) {}
    // ImGui 渲染回调
    virtual void onImGUIRender() {}

    GComponent* getContent() const { return m_root; }
    GComponent* getDisplayContent() const { return m_loadingRoot ? m_loadingRoot : m_root; }
    ViewManager* getViewManager() const { return m_viewManager; }
    bool isLoading() const { return m_state == ViewState::Loading; }
    bool isActive() const { return m_state == ViewState::Active; }

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

    enum class ViewState : uint8_t
    {
        None,
        Loading,
        Active,
    };

    void _create();
    void _destroy();
    bool _updateLoading(float dt);
    void _finishLoading();
    void _createContent();

private:
    GComponent* m_root         = nullptr;
    GComponent* m_loadingRoot  = nullptr;
    ViewManager* m_viewManager = nullptr;

private:
    // 资源加载逻辑
    std::unique_ptr<IResourceLoadOperation> m_loadOperation;
    // 当前界面状态
    ViewState m_state = ViewState::None;
};

}  // namespace gameui
