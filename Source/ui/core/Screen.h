#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>
#include "FairyGUI.h"
#include "GLoader3D.h"
#include "ScreenManager.h"
#include "net/NetErr.h"
#include "net/NetAgent.h"
#include "resource/Resource.h"
#include "resource/ResourceLoader.h"

namespace gameui
{
using namespace fairygui;

class LoadingLayer;

class Screen : public net::NetAgent
{
public:
    Screen();
    virtual ~Screen();

    // 返回依赖的 FairyGUI 包资源路径。
    // 例如 { "UI/Common_Base", "UI/Screen_Login" }
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
    ScreenManager* getScreenManager() const { return m_screenManager; }
    bool isLoading() const { return m_state == ScreenState::Loading; }
    bool isActive() const { return m_state == ScreenState::Active; }

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

protected:
    // 声明进入本 Screen 前要准备的资源；什么都不声明就不会显示加载界面，直接创建正式内容。
    // 默认空实现。只应该在这个函数里调用 getResourceLoader() / setResourceLoader()。
    virtual void onPrepareLoad() {}

    // 本 Screen 专属的加载器，第一次调用时创建，持有到本 Screen 销毁为止。
    gameres::ResourceLoader& getResourceLoader();

    // 使用一个外部共同持有的加载器（例如 AppContext::globalLoader()），和 getResourceLoader()
    // 二选一。本 Screen 只会在它还没 start() 时调用 start()，销毁时只是释放自己的引用，
    // 不会影响其他持有者继续使用它。
    void setResourceLoader(std::shared_ptr<gameres::ResourceLoader> loader);

    // 预加载的资源里有失败项时调用一次。默认打印警告日志并返回 true（不算致命，继续
    // 创建正式内容，运行时按老办法自行处理资源缺失）；返回 false 表示致命，弹窗并结束程序
    // （例如 LaunchScreen 的配置表）。
    virtual bool onResourceLoadFailed(const std::vector<gameres::ResourcePtr>& failedResources);

private:
    friend class ScreenManager;

    enum class ScreenState : uint8_t
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
    void _failFatal(std::string_view message);

private:
    GComponent* m_root         = nullptr;
    GComponent* m_loadingRoot  = nullptr;
    ScreenManager* m_screenManager = nullptr;

private:
    // 加载流程：资源加载器持有到本 Screen 销毁为止（见 getResourceLoader() 的说明）；
    // 加载界面只在 Loading 状态存在，完成后立刻释放。
    std::shared_ptr<gameres::ResourceLoader> m_resourceLoader;
    std::unique_ptr<LoadingLayer> m_loadingLayer;
    bool m_resourcesDone = false;
    bool m_loadFatal     = false;
    // 当前界面状态
    ScreenState m_state = ScreenState::None;
};

}  // namespace gameui
