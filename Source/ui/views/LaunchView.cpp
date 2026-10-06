#include "LaunchView.h"
#include "AppContext.h"
#include "LoginView.h"
#include "ui/widgets/common/MessageDialog.h"

namespace gameui
{

void LaunchView::onEnter()
{
    m_progressBar = getChild<GProgressBar>("progressBar");
    m_progressBar->setValue(0);

    auto* loader = AppContext::get().globalLoader();
    if (loader->isFinished())
    {
        m_loaded = true;
        m_progressBar->tweenValue(m_progressBar->getMax(), 0.3f);
        return;
    }

    // 只在加载完成后才离开本界面，回调里访问 this 是安全的
    loader->start([this](const gameres::LoadProgress& p) {
        m_progressBar->tweenValue(p.percent * m_progressBar->getMax(), 0.3f);
    }, [this](const std::vector<gameres::ResourcePtr>& failed) {
        if (!failed.empty())
        {
            for (const auto& res : failed)
                AXLOGE("LaunchView: failed to load {} '{}': {}", gameres::toString(res->getType()), res->getKey(),
                       res->getError());
            MessageDialog::showGlobal("资源加载失败", []() { ax::Director::getInstance()->end(); });
            return;
        }
        m_loaded = true;
    });
}

void LaunchView::onUpdate(float /*dt*/)
{
    if (!m_loaded || m_progressBar->getValue() < m_progressBar->getMax() ||
        GTween::isTweening(m_progressBar, TweenPropType::Progress))
        return;

    m_loaded = false;
    AppContext::get().connectToDefaultServer();
    getViewManager()->switchView<LoginView>();
}

}  // namespace gameui
