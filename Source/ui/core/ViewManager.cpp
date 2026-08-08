#include "ViewManager.h"
#include "View.h"
#include "UIManager.h"

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
    if (auto* current = getCurrentView())
        current->onUpdate(dt);
    if (m_uiManager)
        m_uiManager->update(dt);

    flushPendingViews();
}

void ViewManager::_queuePending(PendingViewAction action, std::unique_ptr<View> newView)
{
    // 同一帧多次切换：只保留最后一次（丢弃尚未 _create 的 pending）
    m_pendingView   = std::move(newView);
    m_pendingAction = action;
}

void ViewManager::flushPendingViews()
{
    // 新 View 的 onEnter 里若再次 switch/push，继续应用到空为止
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
    // 逐个关闭旧 View 关联的 Widget（保留 Independent 生命周期的 Widget）
    for (auto& view : m_viewStack)
    {
        m_uiManager->closeByOwner(view.get());
    }

    for (auto it = m_viewStack.rbegin(); it != m_viewStack.rend(); ++it)
    {
        _destroyView(*it);
    }
    m_viewStack.clear();

    _pushView(std::move(newView));
}

void ViewManager::_pushView(std::unique_ptr<View> newView)
{
    if (auto* current = getCurrentView())
        _showView(current, false);

    m_viewStack.push_back(std::move(newView));
    auto* current          = getCurrentView();
    current->m_viewManager = this;
    current->_create();
    m_viewLayer->addChild(current->getContent());
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

    if (view->getContent())
        view->getContent()->setVisible(visible);
    if (m_uiManager)
        m_uiManager->setVisibleByOwner(view, visible);
}

}  // namespace gameui
