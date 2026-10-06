#pragma once

#include "ui/core/View.h"

namespace gameui
{

// 启动界面：加载 AppContext 的全局资源（UI/Common、配置表等），完成后连接服务器并进入登录界面。
class LaunchView : public View
{
public:
    // 进度条组件来自 Common 包，所以 Common 需要随本界面同步加载；
    // 全局加载器完成后另外持有一份 Common 引用，退出本界面后 Common 不会被卸载。
    std::vector<std::string> getPackages() const override { return {"UI/Common", "UI/Launch"}; }

    GComponent* onCreateContent() override { return UIPackage::createObject("Launch", "LaunchView")->as<GComponent>(); }

    void onEnter() override;
    void onUpdate(float dt) override;

private:
    GProgressBar* m_progressBar = nullptr;
    bool m_loaded               = false;
};

}  // namespace gameui
