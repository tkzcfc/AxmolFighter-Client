#include "LaunchView.h"
#include "ui/core/ViewManager.h"
#include "mugen/conf/Config.h"
#include "GameView.h"

namespace gameui
{

void LaunchView::onEnter()
{
    m_progressBar = getChild<GProgressBar>("progressBar");
    m_progressBar->setValue(0);

    collectLoadingTasks();

    if (m_taskQueue.empty())
    {
        m_progressBar->tweenValue(100.0, 1.0f);
        m_loadingState = LoadingState::Completed;
    }
    else
    {
        ax::Director::getInstance()->getJobSystem()->enqueue([this]() { this->doLoadingTasks(); }, [this]() {
            m_progressBar->tweenValue(m_progressBar->getMax(), 0.3f);
        });
    }
}

void LaunchView::collectLoadingTasks()
{
    m_taskQueue.push([]() {
        if (!mugen::Config::getInstance()->loadConfig("mugen/config/config.bin"))
        {
            MG_LOG_E("Failed to load file-config registries");
            return false;
        }
        return true;
    });

    m_totalTasks = m_taskQueue.size();
}

void LaunchView::doLoadingTasks()
{
    m_loadingState = LoadingState::InProgress;

    while (!m_taskQueue.empty())
    {
        auto task = m_taskQueue.front();
        m_taskQueue.pop();

#if _DEBUG
        task();
#else
        if (!task())
        {
            m_loadingState = LoadingState::Failed;
            AXLOGE("Failed to execute a loading task.");
            break;
        }
#endif
        m_completedTasks++;
    }
    if (m_loadingState != LoadingState::Failed)
    {
        m_loadingState = LoadingState::Completed;
    }
}

void LaunchView::onUpdate(float /*dt*/)
{
    switch (m_loadingState.load())
    {
    case LoadingState::NotStarted:
        break;
    case LoadingState::InProgress:
        m_progressBar->tweenValue(static_cast<double>(m_completedTasks.load()) / m_totalTasks * m_progressBar->getMax(),
                                  0.3f);
        break;
    case LoadingState::Completed:
        if (m_progressBar->getValue() >= m_progressBar->getMax() &&
            !GTween::isTweening(m_progressBar, TweenPropType::Progress))
        {
            getViewManager()->switchView<GameView>();
        }
        break;
    case LoadingState::Failed:
        // 显示错误信息
        AXLOGE("Failed to load game resources.");
        break;
    }
}

}  // namespace gameui
