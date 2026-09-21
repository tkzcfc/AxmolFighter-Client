#include "View.h"
#include "AppContext.h"
#include "FGUIPackageManager.h"

namespace gameui
{

View::View() {}

View::~View() {}

void View::_create()
{
    m_state = ViewState::None;

    // 有资源加载逻辑
    m_loadOperation = createResourceLoadOperation();
    if (m_loadOperation)
    {
        // 加载 loading 界面所依赖的 FairyGUI 包资源
        FGUIPackageManager::getInstance().load(m_loadOperation->getLoadingPackages());

        // 创建 loading 界面
        m_loadingRoot = m_loadOperation->onCreateLoadingContent();
        m_state        = ViewState::Loading;
        m_loadOperation->start();
        return;
    }

    // 没有资源加载逻辑，直接创建正式内容
    _createContent();
}

void View::_createContent()
{
    FGUIPackageManager::getInstance().load(getPackages());
    GComponent* content = onCreateContent();
    if (content)
    {
        m_root = content;
    }
    else
    {
        m_root = GComponent::create();
        m_root->setSize(GRoot::getInstance()->getWidth(), GRoot::getInstance()->getHeight());
        m_root->addRelation(GRoot::getInstance(), RelationType::Size);
    }

    m_state = ViewState::Active;
    onEnter();
}

void View::_destroy()
{
    detach();

    switch (m_state)
    {
    case ViewState::Loading:
    {
        // 此时应该还没有创建正式内容
        AX_ASSERT(m_root == nullptr);

        if (m_loadingRoot)
        {
            m_loadingRoot->removeFromParent();
            m_loadingRoot = nullptr;
        }

        if (m_loadOperation)
        {
            FGUIPackageManager::getInstance().unload(m_loadOperation->getLoadingPackages());
            m_loadOperation.reset();
        }
        break;
    }
    case ViewState::Active:
    {
        // 确保和 onEnter 成对调用
        onExit();

        // Active 状态时，loadingRoot 和 loadOperation 应该已经被释放
        ASSERT(m_loadingRoot == nullptr && m_loadOperation == nullptr);

        if (m_root)
        {
            m_root->removeFromParent();
            m_root = nullptr;
        }
        FGUIPackageManager::getInstance().unload(getPackages());
        break;
    }
    case ViewState::None:
    {
        // 不应该出现这种情况
        AX_ASSERT(false);
        break;
    }
    };

    m_state = ViewState::None;
}

bool View::_updateLoading(float dt)
{
    AX_ASSERT(m_state == ViewState::Loading);
    return m_loadOperation->update(dt);
}

void View::_finishLoading()
{
    AX_ASSERT(m_state == ViewState::Loading && m_loadOperation != nullptr);

    // 加载完成,移除loading界面
    if (m_loadingRoot)
    {
        m_loadingRoot->removeFromParent();
        m_loadingRoot = nullptr;
    }

    // 卸载 loading 界面所依赖的 FairyGUI 包资源
    FGUIPackageManager::getInstance().unload(m_loadOperation->getLoadingPackages());
    m_loadOperation.reset();

    // 创建正式内容
    _createContent();
}

void View::addClickListener(UIEventDispatcher* dispatcher, const std::function<void(EventContext*)>& callback)
{
    if (dispatcher == nullptr)
    {
        AXLOGW("View::addClickListener: dispatcher is null");
        return;
    }
    dispatcher->addEventListener(UIEventType::Click, callback);
}

}  // namespace gameui
