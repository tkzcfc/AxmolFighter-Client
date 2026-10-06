#include "LaunchView.h"
#include "AppContext.h"
#include "LoginView.h"

namespace gameui
{

void LaunchView::onPrepareLoad()
{
    // 和 AppContext 共同持有：本 View 销毁后全局资源不会被释放，直到 AppContext 自己销毁。
    setResourceLoader(AppContext::get().globalLoader());
}

bool LaunchView::onResourceLoadFailed(const std::vector<gameres::ResourcePtr>& failedResources)
{
    for (const auto& res : failedResources)
    {
        AXLOGE("LaunchView: failed to load {} '{}': {}", gameres::toString(res->getType()), res->getKey(),
               res->getError());
    }
    return false;
}

void LaunchView::onEnter()
{
    // 走到这里时全局资源已经加载完成
    AppContext::get().connectToDefaultServer();
    getViewManager()->switchView<LoginView>();
}

}  // namespace gameui
