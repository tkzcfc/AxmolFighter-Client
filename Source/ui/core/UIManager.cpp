#include "UIManager.h"
#include "UIWidget.h"
#include "ViewManager.h"
#include <algorithm>
#include <limits>

namespace gameui
{

UIManager::UIManager() {}

UIManager::~UIManager()
{
    closeAll();
}

void UIManager::init(ViewManager* viewManager, GComponent* layer)
{
    m_viewManager = viewManager;
    m_layer       = layer;
}

void UIManager::update(float dt)
{
    auto snapshot = m_widgets;
    for (auto& w : snapshot)
    {
        if (_isActiveVisible(w.get()))
            w->onUpdate(dt);
    }
}

void UIManager::_open(std::shared_ptr<UIWidget> widget, const UIOpenOptions& options)
{
    AXASSERT(m_nextOpenOrder != std::numeric_limits<uint16_t>::max(), "UIManager open order overflow");

    const uint16_t openOrder = m_nextOpenOrder++;
    const uint32_t zorder    = (static_cast<uint32_t>(options.layer) << 16) | openOrder;
    AXASSERT(zorder <= static_cast<uint32_t>(std::numeric_limits<int>::max()),
             "UI zorder exceeds FairyGUI sortingOrder range");

    UIWidget* ptr = widget.get();
    ptr->_create();
    ptr->getRoot()->setSortingOrder(static_cast<int>(zorder));
    m_layer->addChild(ptr->getRoot());
    ptr->m_manager      = this;
    ptr->m_ownerView    = m_viewManager ? m_viewManager->getCurrentView() : nullptr;
    ptr->m_lifecycle    = options.lifecycle;
    ptr->m_zorder       = zorder;
    ptr->m_stackVisible = true;

    auto insertPos =
        std::upper_bound(m_widgets.begin(), m_widgets.end(), zorder,
                         [](uint32_t value, const std::shared_ptr<UIWidget>& item) { return value < item->m_zorder; });
    m_widgets.insert(insertPos, std::move(widget));

    ptr->_doShow();

    // 优化操作,如果打开的是全屏控件,则可以隐藏这个控件下面的所有控件,避免不必要的渲染
    _notifyFullscreenChange();

    // 更新背景状态,确保只有最上层的控件显示背景
    _updateBackgrounds();
}

void UIManager::close(UIWidget* widget)
{
    if (!widget)
        return;
    widget->close();
}

void UIManager::closeAll()
{
    // 销毁所有控件，无动画效果
    for (auto& w : m_widgets)
    {
        w->m_manager   = nullptr;
        w->m_ownerView = nullptr;
        w->_destroy();
    }
    m_widgets.clear();
    m_nextOpenOrder = 0;
}

void UIManager::closeByOwner(View* owner)
{
    for (auto it = m_widgets.begin(); it != m_widgets.end();)
    {
        if ((*it)->m_ownerView == owner)
        {
            // 如果 Widget 设置为独立生命周期，则不跟随 View 销毁
            if ((*it)->m_lifecycle == UIWidgetLifecycle::Independent)
            {
                (*it)->m_ownerView = nullptr;
                ++it;
                continue;
            }

            (*it)->m_manager   = nullptr;
            (*it)->m_ownerView = nullptr;
            (*it)->_destroy();
            it = m_widgets.erase(it);
        }
        else
        {
            ++it;
        }
    }
    _updateBackgrounds();
    _notifyFullscreenChange();
}

void UIManager::setVisibleByOwner(View* owner, bool visible)
{
    for (auto& w : m_widgets)
    {
        if (w->m_ownerView != owner)
            continue;

        w->m_stackVisible = visible;
        if (!visible && w->getRoot())
            w->getRoot()->setVisible(false);
    }
    _notifyFullscreenChange();
    _updateBackgrounds();
}

UIWidget* UIManager::getTopWidget() const
{
    for (auto it = m_widgets.rbegin(); it != m_widgets.rend(); ++it)
    {
        if (_isActiveVisible(it->get()))
            return it->get();
    }
    return nullptr;
}

bool UIManager::hasVisibleWidget() const
{
    for (auto& w : m_widgets)
    {
        if (_isActiveVisible(w.get()))
            return true;
    }
    return false;
}

ViewManager* UIManager::getViewManager() const
{
    return m_viewManager;
}

bool UIManager::_isActiveVisible(const UIWidget* widget) const
{
    return widget && widget->m_stackVisible && widget->isVisible();
}

void UIManager::_updateBackgrounds()
{
    bool topFound = false;
    for (auto it = m_widgets.rbegin(); it != m_widgets.rend(); ++it)
    {
        UIWidget* w = it->get();
        if (!_isActiveVisible(w) || !w->hasBackground())
        {
            w->setBackgroundVisible(false);
            continue;
        }

        if (!topFound)
        {
            w->setBackgroundVisible(true);
            topFound = true;
        }
        else
        {
            w->setBackgroundVisible(false);
        }
    }
}

void UIManager::_notifyFullscreenChange()
{
    UIWidget* top = getTopWidget();
    if (top && top->isFullscreen())
    {
        top->getRoot()->setVisible(true);
        for (auto& w : m_widgets)
        {
            if (w.get() != top && _isActiveVisible(w.get()))
            {
                w->getRoot()->setVisible(false);
            }
        }
    }
    else
    {
        for (auto& w : m_widgets)
        {
            if (w->m_stackVisible && (w->getState() == UIState::Visible || w->getState() == UIState::Showing))
            {
                w->getRoot()->setVisible(true);
            }
        }
    }
}

// 由 UIWidget 在隐藏动画完成后调用
void UIManager::_onWidgetHidden(UIWidget* widget)
{
    _removeWidget(widget);
    _updateBackgrounds();
    // 如果之前是全屏控件由于优化操作将其他控件隐藏了，则需要通知其他控件恢复显示
    _notifyFullscreenChange();
}

void UIManager::_removeWidget(UIWidget* widget)
{
    auto it = std::find_if(m_widgets.begin(), m_widgets.end(),
                           [widget](const std::shared_ptr<UIWidget>& w) { return w.get() == widget; });
    if (it != m_widgets.end())
    {
        // 断开控件与 UIManager 的关联，避免在销毁过程中 UIManager 再次访问控件
        (*it)->m_manager   = nullptr;
        (*it)->m_ownerView = nullptr;
        // 直接销毁控件实例，避免等待 shared_ptr 的引用计数归零
        (*it)->_destroy();
        m_widgets.erase(it);
    }
}

}  // namespace gameui
