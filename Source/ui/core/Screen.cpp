#include "Screen.h"
#include "FGUIPackageManager.h"
#include "ui/widgets/common/LoadingLayer.h"
#include "ui/widgets/common/MessageDialog.h"

namespace gameui
{

Screen::Screen() {}

Screen::~Screen() {}

gameres::ResourceLoader& Screen::getResourceLoader()
{
    if (!m_resourceLoader)
        m_resourceLoader = std::make_shared<gameres::ResourceLoader>();
    return *m_resourceLoader;
}

void Screen::setResourceLoader(std::shared_ptr<gameres::ResourceLoader> loader)
{
    m_resourceLoader = std::move(loader);
}

bool Screen::onResourceLoadFailed(const std::vector<gameres::ResourcePtr>& failedResources)
{
    for (const auto& res : failedResources)
    {
        AXLOGW("Screen: failed to load {} '{}': {}", gameres::toString(res->getType()), res->getKey(), res->getError());
    }
    // 默认不算致命：继续创建正式内容，运行时按老办法自行处理资源缺失
    return true;
}

void Screen::_create()
{
    m_state = ScreenState::None;

    onPrepareLoad();

    if (!m_resourceLoader)
    {
        _createContent();
        return;
    }

    m_loadingLayer = std::make_unique<LoadingLayer>();
    m_loadingRoot  = m_loadingLayer->getRoot();
    m_state        = ScreenState::Loading;
}

void Screen::_createContent()
{
    FGUIPackageManager::getInstance().load(getPackages());
    GComponent* content = onCreateContent();
    m_root              = content ? content : GComponent::create();
    m_root->setSize(GRoot::getInstance()->getWidth(), GRoot::getInstance()->getHeight());
    m_root->addRelation(GRoot::getInstance(), RelationType::Size);

    m_state = ScreenState::Active;
    onEnter();
}

void Screen::_destroy()
{
    detach();

    switch (m_state)
    {
    case ScreenState::Loading:
    {
        // 此时应该还没有创建正式内容
        AX_ASSERT(m_root == nullptr);

        if (m_loadingRoot)
        {
            m_loadingRoot->removeFromParent();
            m_loadingRoot = nullptr;
        }
        m_loadingLayer.reset();
        break;
    }
    case ScreenState::Active:
    {
        // 确保和 onEnter 成对调用
        onExit();

        // Active 状态时，loadingRoot 和加载界面应该已经被释放
        AXASSERT(m_loadingRoot == nullptr && m_loadingLayer == nullptr,
                 "loading state must be released before an active screen unloads");

        if (m_root)
        {
            m_root->removeFromParent();
            m_root = nullptr;
        }
        FGUIPackageManager::getInstance().unload(getPackages());
        break;
    }
    case ScreenState::None:
    {
        // 不应该出现这种情况
        AX_ASSERT(false);
        break;
    }
    };

    // 加载器持有到这里才释放：
    // - 自己 getResourceLoader() 创建的那份，析构时会立即释放持有的全部资源；
    // - setResourceLoader() 借用的那份，这里只是释放本 Screen 的引用，不影响其他持有者
    //   （例如 AppContext::globalLoader()）继续使用。
    m_resourceLoader.reset();

    m_state = ScreenState::None;
}

bool Screen::_updateLoading(float dt)
{
    AX_ASSERT(m_state == ScreenState::Loading);
    AX_ASSERT(m_resourceLoader);

    if (m_loadFatal)
        return false;

    if (!m_resourcesDone)
    {
        if (!m_resourceLoader->isStarted())
            m_resourceLoader->start(nullptr, nullptr);

        m_loadingLayer->setTarget(m_resourceLoader->getProgress());

        if (!m_resourceLoader->isFinished())
        {
            m_loadingLayer->update(dt);
            return false;
        }

        if (auto failed = m_resourceLoader->getFailedResources(); !failed.empty())
        {
            if (!onResourceLoadFailed(failed))
            {
                _failFatal("资源加载失败");
                m_loadingLayer->update(dt);
                return false;
            }
        }
        m_resourcesDone = true;
    }

    m_loadingLayer->finish();
    m_loadingLayer->update(dt);
    return m_loadingLayer->isDone();
}

void Screen::_finishLoading()
{
    AX_ASSERT(m_state == ScreenState::Loading && m_loadingLayer != nullptr);

    // 加载完成,移除loading界面
    if (m_loadingRoot)
    {
        m_loadingRoot->removeFromParent();
        m_loadingRoot = nullptr;
    }
    m_loadingLayer.reset();

    // 创建正式内容（资源加载器继续持有，直到 _destroy()）
    _createContent();
}

void Screen::_failFatal(std::string_view message)
{
    if (m_loadFatal)
        return;
    m_loadFatal = true;
    MessageDialog::showGlobal(message, []() { ax::Director::getInstance()->end(); });
}

void Screen::addClickListener(UIEventDispatcher* dispatcher, const std::function<void(EventContext*)>& callback)
{
    if (dispatcher == nullptr)
    {
        AXLOGW("Screen::addClickListener: dispatcher is null");
        return;
    }
    dispatcher->addEventListener(UIEventType::Click, callback);
}

}  // namespace gameui
