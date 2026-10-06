#pragma once

#include "ui/core/View.h"

namespace gameui
{

// 启动界面：加载 AppContext 的全局资源（配置表等），完成后连接服务器并进入登录界面。
// 本身没有正式内容，加载界面用的是 Common 包里的通用 LoadingLayer（见 ui/core/View.h）。
class LaunchView : public View
{
protected:
    void onPrepareLoad() override;

    // 全局资源（配置表）加载失败视为致命：没有配置就没法进游戏
    bool onResourceLoadFailed(const std::vector<gameres::ResourcePtr>& failedResources) override;

public:
    void onEnter() override;
};

}  // namespace gameui
