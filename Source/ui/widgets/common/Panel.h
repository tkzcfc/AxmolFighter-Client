#pragma once

#include "ui/core/UIWidget.h"

namespace gameui
{

// 全屏功能界面基类。子类在 onCreatePanelContent() 里返回创建好的组件，
// 基类会像 Screen 一样把它铺满整个界面并关联尺寸，屏幕尺寸变化时自动重新布局。
class Panel : public UIWidget
{
public:
    Panel();

    virtual ~Panel();

protected:
    // 子类创建全屏组件
    virtual GComponent* onCreatePanelContent() = 0;

    virtual GComponent* onCreateContent() override final;
};

}  // namespace gameui
