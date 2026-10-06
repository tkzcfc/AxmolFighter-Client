#include "ViewManager.h"

#include "UIManager.h"
#include "View.h"

#include "mugen/render/spine/SpineSkeletonCache.h"

using namespace fairygui;

namespace gameui
{

ViewManager::ViewManager() {}

ViewManager::~ViewManager()
{
    m_pendingView.reset();
    m_pendingAction = PendingViewAction::None;

    if (m_uiManager)
        m_uiManager->closeAll();

    for (auto it = m_viewStack.rbegin(); it != m_viewStack.rend(); ++it)
    {
        _destroyView(*it);
    }
    m_viewStack.clear();

    if (m_groot)
    {
        m_groot->release();
        m_groot = nullptr;
    }
}

void ViewManager::init(ax::Scene* scene)
{
    m_groot = GRoot::create(scene);
    m_groot->retain();

    m_viewLayer = GComponent::create();
    m_viewLayer->setSize(m_groot->getWidth(), m_groot->getHeight());
    m_viewLayer->addRelation(m_groot, RelationType::Size);
    m_groot->addChild(m_viewLayer);

    m_uiLayer = GComponent::create();
    m_uiLayer->setSize(m_groot->getWidth(), m_groot->getHeight());
    m_uiLayer->addRelation(m_groot, RelationType::Size);
    m_groot->addChild(m_uiLayer);

    m_uiManager = std::make_unique<UIManager>();
    m_uiManager->init(this, m_uiLayer);
}

void ViewManager::update(float dt)
{
    flushPendingViews();

    if (View* current = getCurrentView())
    {
        // 如果当前 View 正在加载资源，则调用 _updateLoading；如果加载完成，则调用 _finishLoading
        if (current->isLoading())
        {
            if (current->_updateLoading(dt))
            {
                // 确保当前 View 仍然是栈顶 View，防止在_updateLoading中切换了 View
                AX_ASSERT(current == getCurrentView());

                current->_finishLoading();
                if (auto* content = current->getContent())
                    m_viewLayer->addChild(content);
            }
        }
        else
        {
            current->onUpdate(dt);
        }
    }

    if (m_uiManager)
        m_uiManager->update(dt);

    flushPendingViews();
}

void ViewManager::_queuePending(PendingViewAction action, std::unique_ptr<View> newView)
{
    if (isLoading())
    {
        AXLOGW("ViewManager: ignore view transition while current View is loading");
        return;
    }

    m_pendingView   = std::move(newView);
    m_pendingAction = action;
}

bool ViewManager::isLoading() const
{
    auto* current = getCurrentView();
    return current && current->isLoading();
}

void ViewManager::flushPendingViews()
{
    while (m_pendingAction != PendingViewAction::None)
    {
        const PendingViewAction action = m_pendingAction;
        std::unique_ptr<View> view     = std::move(m_pendingView);
        m_pendingAction                = PendingViewAction::None;

        if (!view)
            continue;

        if (action == PendingViewAction::Switch)
            _switchView(std::move(view));
        else if (action == PendingViewAction::Push)
            _pushView(std::move(view));
    }
}

void ViewManager::_switchView(std::unique_ptr<View> newView)
{
    if (m_uiManager)
    {
        for (auto it = m_viewStack.rbegin(); it != m_viewStack.rend(); ++it)
        {
            m_uiManager->closeByOwner(it->get());
        }
    }

    for (auto it = m_viewStack.rbegin(); it != m_viewStack.rend(); ++it)
    {
        _destroyView(*it);
    }
    m_viewStack.clear();

    _pushView(std::move(newView));

    // 旧 View 的节点下一帧才会真正释放，届时回收无人引用的共享 spine 数据
    ax::Director::getInstance()->getScheduler()->runOnAxmolThread(
        []() { mugen::SpineSkeletonCache::getInstance()->purgeUnused(); });
}

void ViewManager::_pushView(std::unique_ptr<View> newView)
{
    if (auto* current = getCurrentView())
        _showView(current, false);

    m_viewStack.push_back(std::move(newView));
    auto* current          = getCurrentView();
    current->m_viewManager = this;
    current->_create();

    if (auto* displayContent = current->getDisplayContent())
        m_viewLayer->addChild(displayContent);
}

bool ViewManager::popView()
{
    if (m_viewStack.size() <= 1)
        return false;

    auto& top = m_viewStack.back();
    if (m_uiManager)
        m_uiManager->closeByOwner(top.get());
    _destroyView(top);
    m_viewStack.pop_back();

    _showView(getCurrentView(), true);
    return true;
}

void ViewManager::_destroyView(std::unique_ptr<View>& view)
{
    if (!view)
        return;

    view->_destroy();
    view->m_viewManager = nullptr;
}

void ViewManager::_showView(View* view, bool visible)
{
    if (!view)
        return;

    if (auto* displayContent = view->getDisplayContent())
        displayContent->setVisible(visible);
    if (m_uiManager)
        m_uiManager->setVisibleByOwner(view, visible);
}

}  // namespace gameui
