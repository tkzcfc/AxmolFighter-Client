#include "View.h"
#include "AppContext.h"
#include "FGUIPackageManager.h"
#include "ui/widgets/common/LoadingLayer.h"
#include "ui/widgets/common/MessageDialog.h"

namespace gameui
{

namespace
{
// 初始化步骤在总进度中的占比；资源加载占剩下的部分。
constexpr float kStepShare = 0.1f;
}  // namespace

View::View() {}

View::~View() {}

gameres::ResourceLoader& View::getResourceLoader()
{
    if (!m_resourceLoader)
        m_resourceLoader = std::make_shared<gameres::ResourceLoader>();
    return *m_resourceLoader;
}

void View::setResourceLoader(std::shared_ptr<gameres::ResourceLoader> loader)
{
    m_resourceLoader = std::move(loader);
}

void View::addLoadStep(std::function<bool()> step)
{
    m_loadSteps.push_back(std::move(step));
}

bool View::onResourceLoadFailed(const std::vector<gameres::ResourcePtr>& failedResources)
{
    for (const auto& res : failedResources)
    {
        AXLOGW("View: failed to load {} '{}': {}", gameres::toString(res->getType()), res->getKey(), res->getError());
    }
    // 默认不算致命：继续往下执行初始化步骤，运行时按老办法自行处理资源缺失
    return true;
}

void View::_create()
{
    m_state = ViewState::None;

    onPrepareLoad();

    if (!m_resourceLoader && m_loadSteps.empty())
    {
        // 没有资源加载逻辑，直接创建正式内容
        _createContent();
        return;
    }

    m_loadingLayer = std::make_unique<LoadingLayer>();
    m_loadingRoot  = m_loadingLayer->getRoot();
    m_state        = ViewState::Loading;
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
        m_loadingLayer.reset();
        m_loadSteps.clear();
        break;
    }
    case ViewState::Active:
    {
        // 确保和 onEnter 成对调用
        onExit();

        // Active 状态时，loadingRoot 和加载界面应该已经被释放
        ASSERT(m_loadingRoot == nullptr && m_loadingLayer == nullptr);

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

    // 加载器持有到这里才释放：
    // - 自己 getResourceLoader() 创建的那份，析构时会立即释放持有的全部资源；
    // - setResourceLoader() 借用的那份，这里只是释放本 View 的引用，不影响其他持有者
    //   （例如 AppContext::globalLoader()）继续使用。
    m_resourceLoader.reset();

    m_state = ViewState::None;
}

bool View::_updateLoading(float dt)
{
    AX_ASSERT(m_state == ViewState::Loading);

    if (m_loadFatal)
        return false;

    if (!m_resourcesDone)
    {
        if (m_resourceLoader)
        {
            if (!m_resourceLoader->isStarted())
                m_resourceLoader->start(nullptr, nullptr);

            const float resourceShare = m_loadSteps.empty() ? 1.0f : (1.0f - kStepShare);
            m_loadingLayer->setTarget(m_resourceLoader->getProgress() * resourceShare);

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
        }
        m_resourcesDone = true;
    }

    if (m_loadStepIndex < m_loadSteps.size())
    {
        if (!m_loadSteps[m_loadStepIndex]())
        {
            _failFatal("初始化失败");
            m_loadingLayer->update(dt);
            return false;
        }
        ++m_loadStepIndex;
    }

    const float resourceShare = m_resourceLoader ? (1.0f - kStepShare) : 0.0f;
    const float stepShare     = m_resourceLoader ? kStepShare : 1.0f;
    const float stepProgress =
        m_loadSteps.empty() ? 1.0f : static_cast<float>(m_loadStepIndex) / static_cast<float>(m_loadSteps.size());
    m_loadingLayer->setTarget(resourceShare + stepShare * stepProgress);

    if (m_loadStepIndex >= m_loadSteps.size())
        m_loadingLayer->finish();

    m_loadingLayer->update(dt);
    return m_loadingLayer->isDone();
}

void View::_finishLoading()
{
    AX_ASSERT(m_state == ViewState::Loading && m_loadingLayer != nullptr);

    // 加载完成,移除loading界面
    if (m_loadingRoot)
    {
        m_loadingRoot->removeFromParent();
        m_loadingRoot = nullptr;
    }
    m_loadingLayer.reset();
    m_loadSteps.clear();
    m_loadStepIndex = 0;

    // 创建正式内容（资源加载器继续持有，直到 _destroy()）
    _createContent();
}

void View::_failFatal(std::string_view message)
{
    if (m_loadFatal)
        return;
    m_loadFatal = true;
    MessageDialog::showGlobal(message, []() { ax::Director::getInstance()->end(); });
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
