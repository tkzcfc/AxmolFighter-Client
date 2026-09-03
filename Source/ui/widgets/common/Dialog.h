#pragma once

#include "ui/core/UIWidget.h"

namespace gameui
{

class Dialog : public UIWidget
{
public:
    Dialog();

    virtual ~Dialog();

    // 在销毁时调用,子类可以停止自定义的显示/隐藏动画
    virtual void onStopAnimations() override;

    // 播放显示动画，done 回调必须且只能被调用一次，调用时机由子类决定
    virtual void doShowAnimation(std::function<void()> done) override;

    // 播放隐藏动画，done 回调必须且只能被调用一次，调用时机由子类决定
    virtual void doHideAnimation(std::function<void()> done) override;
};

}  // namespace gameui
