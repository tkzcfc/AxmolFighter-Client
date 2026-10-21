#include "LaunchScreen.h"
#include "AppContext.h"
#include "LoginScreen.h"

namespace gameui
{

void LaunchScreen::onPrepareLoad()
{
    // 和 AppContext 共同持有：本 Screen 销毁后全局资源不会被释放，直到 AppContext 自己销毁。
    setResourceLoader(AppContext::get().globalLoader());
}

bool LaunchScreen::onResourceLoadFailed(const std::vector<gameres::ResourcePtr>& failedResources)
{
    for (const auto& res : failedResources)
    {
        AXLOGE("LaunchScreen: failed to load {} '{}': {}", gameres::toString(res->getType()), res->getKey(),
               res->getError());
    }
    return false;
}

void LaunchScreen::onEnter()
{
    // 走到这里时全局资源已经加载完成
    AppContext::get().connectToDefaultServer();
    getScreenManager()->switchScreen<LoginScreen>();
}

}  // namespace gameui
