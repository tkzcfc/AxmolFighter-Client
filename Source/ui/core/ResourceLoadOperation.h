#pragma once

#include "FairyGUI.h"

#include <string>
#include <vector>

namespace gameui
{

using namespace fairygui;

// View 切换期间执行的一次资源/场景准备流程。
// Operation 自己维护加载状态并更新自己的 Loading 界面；ViewManager
// 只负责调用 start/update，并在 update 返回 true 后激活 View。
class IResourceLoadOperation
{
public:
    virtual ~IResourceLoadOperation() = default;

    // Loading 界面所依赖的 FairyGUI 包。
    virtual std::vector<std::string> getLoadingPackages() const { return {}; }

    // Operation 创建的 Loading 内容由 View 挂载和销毁，Operation 只保存非拥有引用。
    virtual GComponent* onCreateLoadingContent() = 0;

    // 只调用一次，且在主线程执行。
    virtual void start() = 0;

    // 每帧在主线程执行；返回 true 表示可以进入正式 View。
    virtual bool update(float dt) = 0;
};

}  // namespace gameui
