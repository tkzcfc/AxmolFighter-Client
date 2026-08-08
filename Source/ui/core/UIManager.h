#pragma once

#include "FairyGUI.h"
#include "UITypes.h"
#include <memory>
#include <utility>
#include <vector>

namespace gameui
{

using namespace fairygui;

class View;
class ViewManager;
class UIWidget;

class UIManager
{
public:
    UIManager();
    ~UIManager();

    void init(ViewManager* viewManager, GComponent* layer);
    void update(float dt);

    template <typename T, typename... Args>
    std::weak_ptr<T> open(Args&&... args)
    {
        return openWithOptions<T>(UIOpenOptions{}, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    std::weak_ptr<T> openWithOptions(const UIOpenOptions& options, Args&&... args)
    {
        auto widget = std::make_shared<T>(std::forward<Args>(args)...);
        _open(widget, options);
        return widget;
    }

    void close(UIWidget* widget);
    void closeAll();
    void closeByOwner(View* owner);
    void setVisibleByOwner(View* owner, bool visible);

    UIWidget* getTopWidget() const;
    bool hasVisibleWidget() const;

    ViewManager* getViewManager() const;

private:
    void _open(std::shared_ptr<UIWidget> widget, const UIOpenOptions& options);
    bool _isActiveVisible(const UIWidget* widget) const;
    void _updateBackgrounds();
    void _onWidgetHidden(UIWidget* widget);
    void _notifyFullscreenChange();
    void _removeWidget(UIWidget* widget);

    friend class UIWidget;

    ViewManager* m_viewManager = nullptr;
    GComponent* m_layer        = nullptr;
    std::vector<std::shared_ptr<UIWidget>> m_widgets;
    uint16_t m_nextOpenOrder = 0;
};

}  // namespace gameui
