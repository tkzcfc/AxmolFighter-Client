#pragma once

#include <cstdint>

namespace gameui
{

// ============================================================
// UI 状态枚举
// ============================================================
enum class UIState
{
    None,
    Showing,
    Visible,
    Hiding,
    Hidden
};

// ============================================================
// Widget 生命周期枚举
// ============================================================
enum class UIWidgetLifecycle
{
    WithOwner,    // 跟随所属View一起销毁（默认）
    Independent,  // 独立存在，不随View销毁
};

// ============================================================
// Widget 打开选项
// ============================================================
struct UIOpenOptions
{
    uint16_t layer              = 0;
    UIWidgetLifecycle lifecycle = UIWidgetLifecycle::WithOwner;
};

// ============================================================
// Widget 属性选项
// ============================================================
struct UIWidgetOptions
{
    bool hasBackground        = false;  // 是否有背景遮罩
    uint8_t backgroundOpacity = 153;    // 背景遮罩不透明度, 0-255,默认153(60%不透明)
    bool draggable            = false;  // 是否可拖动
    bool fullscreen     = false;  // 是否是全屏UI界面,全屏界面会触发特殊优化,例如在打开新界面时隐藏旧界面以节省性能
    bool closeOnClickBg = false;  // 点击背景遮罩时关闭
};

// ============================================================
// zorder 相关常量
// zorder 由 layer 和 open order 组成，layer 占高 16 位，open order 占低 16 位
// zorder 是一个int32_t类型而不是uint32_t类型所以不能超过 0x7FFFFFFF，layer 的最大值是 0x7FFF
// ============================================================
constexpr uint16_t UI_OPTIONS_LAYER_MAXVALUE = 0x7FFF;

}  // namespace gameui
