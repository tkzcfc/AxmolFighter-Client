#include "ScreenManager.h"

#include "UIManager.h"
#include "Screen.h"

#include "mugen/render/spine/SpineSkeletonCache.h"

using namespace fairygui;

namespace gameui
{

ScreenManager::ScreenManager() {}

ScreenManager::~ScreenManager()
{
    m_pendingScreen.reset();
    m_pendingAction = PendingScreenAction::None;

    if (m_uiManager)
        m_uiManager->closeAll();

    for (auto it = m_screenStack.rbegin(); it != m_screenStack.rend(); ++it)
    {
        _destroyScreen(*it);
    }
    m_screenStack.clear();

    if (m_groot)
    {
        m_groot->release();
        m_groot = nullptr;
    }
}

void ScreenManager::init(ax::Scene* scene)
{
    m_groot = GRoot::create(scene);
    m_groot->retain();

    m_screenLayer = GComponent::create();
    m_screenLayer->setSize(m_groot->getWidth(), m_groot->getHeight());
    m_screenLayer->addRelation(m_groot, RelationType::Size);
    m_groot->addChild(m_screenLayer);

    m_uiLayer = GComponent::create();
    m_uiLayer->setSize(m_groot->getWidth(), m_groot->getHeight());
    m_uiLayer->addRelation(m_groot, RelationType::Size);
    m_groot->addChild(m_uiLayer);

    m_uiManager = std::make_unique<UIManager>();
    m_uiManager->init(this, m_uiLayer);
}

void ScreenManager::update(float dt)
{
    flushPendingScreens();

    if (Screen* current = getCurrentScreen())
    {
        // 如果当前 Screen 正在加载资源，则调用 _updateLoading；如果加载完成，则调用 _finishLoading
        if (current->isLoading())
        {
            if (current->_updateLoading(dt))
            {
                // 确保当前 Screen 仍然是栈顶 Screen，防止在_updateLoading中切换了 Screen
                AX_ASSERT(current == getCurrentScreen());

                current->_finishLoading();
                if (auto* content = current->getContent())
                    m_screenLayer->addChild(content);
            }
        }
        else
        {
            current->onUpdate(dt);

            // 新 Screen 已经进入 Active：它这时已经把自己要用的共享数据标记为"在用"，
            // 现在清理才不会把它刚预热、还没来得及引用的数据当成"无人引用"误删
            if (m_purgeSpineCachePending)
            {
                m_purgeSpineCachePending = false;
                ax::Director::getInstance()->getScheduler()->runOnAxmolThread(
                    []() { mugen::SpineSkeletonCache::getInstance()->purgeUnused(); });
            }
        }
    }

    if (m_uiManager)
        m_uiManager->update(dt);

    flushPendingScreens();
}

void ScreenManager::_queuePending(PendingScreenAction action, std::unique_ptr<Screen> newScreen)
{
    if (isLoading())
    {
        AXLOGW("ScreenManager: ignore screen transition while current Screen is loading");
        return;
    }

    m_pendingScreen   = std::move(newScreen);
    m_pendingAction = action;
}

bool ScreenManager::isLoading() const
{
    auto* current = getCurrentScreen();
    return current && current->isLoading();
}

void ScreenManager::flushPendingScreens()
{
    while (m_pendingAction != PendingScreenAction::None)
    {
        const PendingScreenAction action = m_pendingAction;
        std::unique_ptr<Screen> screen     = std::move(m_pendingScreen);
        m_pendingAction                = PendingScreenAction::None;

        if (!screen)
            continue;

        if (action == PendingScreenAction::Switch)
            _switchScreen(std::move(screen));
        else if (action == PendingScreenAction::Push)
            _pushScreen(std::move(screen));
    }
}

void ScreenManager::_switchScreen(std::unique_ptr<Screen> newScreen)
{
    if (m_uiManager)
    {
        for (auto it = m_screenStack.rbegin(); it != m_screenStack.rend(); ++it)
        {
            m_uiManager->closeByOwner(it->get());
        }
    }

    for (auto it = m_screenStack.rbegin(); it != m_screenStack.rend(); ++it)
    {
        _destroyScreen(*it);
    }
    m_screenStack.clear();

    _pushScreen(std::move(newScreen));

    // 旧 Screen 的节点下一帧才会真正释放；清理本身也要延后到新 Screen 进入 Active 之后再做
    // （见 update() 里的说明），避免新 Screen 还在异步加载时，它刚预热、尚未被任何节点
    // 引用的共享 Spine 数据被这里的清理当成"无人引用"误删，又得重新加载一遍。
    m_purgeSpineCachePending = true;
}

void ScreenManager::_pushScreen(std::unique_ptr<Screen> newScreen)
{
    if (auto* current = getCurrentScreen())
        _showScreen(current, false);

    m_screenStack.push_back(std::move(newScreen));
    auto* current          = getCurrentScreen();
    current->m_screenManager = this;
    current->_create();

    if (auto* displayContent = current->getDisplayContent())
        m_screenLayer->addChild(displayContent);
}

bool ScreenManager::popScreen()
{
    if (m_screenStack.size() <= 1)
        return false;

    auto& top = m_screenStack.back();
    if (m_uiManager)
        m_uiManager->closeByOwner(top.get());
    _destroyScreen(top);
    m_screenStack.pop_back();

    _showScreen(getCurrentScreen(), true);
    return true;
}

void ScreenManager::_destroyScreen(std::unique_ptr<Screen>& screen)
{
    if (!screen)
        return;

    screen->_destroy();
    screen->m_screenManager = nullptr;
}

void ScreenManager::_showScreen(Screen* screen, bool visible)
{
    if (!screen)
        return;

    if (auto* displayContent = screen->getDisplayContent())
        displayContent->setVisible(visible);
    if (m_uiManager)
        m_uiManager->setVisibleByOwner(screen, visible);
}

}  // namespace gameui
