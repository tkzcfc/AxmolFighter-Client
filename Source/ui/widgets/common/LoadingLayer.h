#pragma once

#include "ProgressSmoother.h"

#include "FairyGUI.h"

namespace gameui
{
using namespace fairygui;

// 通用加载界面：封装 Common 包里的 LoadingLayer 组件和其中的 progressBar 节点。
// UI/Common 是全局同步加载的（见 AppContext::create），这里不负责包的加载/卸载。
// 不会自己挂到任何父节点上，由调用方（gameui::View）把 getRoot() 加到场景里并负责销毁。
class LoadingLayer
{
public:
    LoadingLayer();

    GComponent* getRoot() const { return m_root; }

    // 当前的真实进度（0..1），每帧由调用方设置
    void setTarget(float progress01);
    // 标记真实加载已经全部完成；之后 update() 会让显示进度加速冲到 100%
    void finish();

    void update(float dt);
    // 显示进度是否也已经走到 100%（不等于真实加载是否完成，可能还在收尾动画中）
    bool isDone() const { return m_smoother.isDone(); }

private:
    GComponent* m_root          = nullptr;
    GProgressBar* m_progressBar = nullptr;
    ProgressSmoother m_smoother;
    float m_target  = 0.0f;
    bool m_finished = false;
};

}  // namespace gameui
