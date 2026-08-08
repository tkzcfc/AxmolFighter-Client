#pragma once

#include "ui/core/View.h"

namespace gameui
{

class LaunchView : public View
{
public:
    std::vector<std::string> getPackages() const override { return {"UI/Launch"}; }

    GComponent* onCreateContent() override { return UIPackage::createObject("Launch", "LaunchView")->as<GComponent>(); }

    virtual void onEnter() override;
    virtual void onUpdate(float /*dt*/) override;

private:
    void collectLoadingTasks();

    void doLoadingTasks();

private:
    // 加载状态
    enum class LoadingState
    {
        NotStarted,
        InProgress,
        Completed,
        Failed,
    };

    std::queue<std::function<bool()>> m_taskQueue;
    GProgressBar* m_progressBar = nullptr;

    // 当前加载状态
    std::atomic<LoadingState> m_loadingState{LoadingState::NotStarted};
    // 当前完成的任务数量
    std::atomic<size_t> m_completedTasks = 0;
    // 任务总数
    size_t m_totalTasks = 0;
};

}  // namespace gameui
