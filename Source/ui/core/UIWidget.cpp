#include "UIWidget.h"
#include "AppContext.h"
#include "FGUIPackageManager.h"

using namespace fairygui;

namespace
{
#ifdef _DEBUG
// 确保回调函数只被调用一次,debug 模式下如果被调用多次会触发断言,release 模式下则只会执行第一次调用
std::function<void()> onceCallback(std::function<void()> fn, const char* msg)
{
    auto called = std::make_shared<bool>(false);
    return [called, fn = std::move(fn), msg]() {
        AXASSERT(!*called, msg);
        *called = true;
        fn();
    };
}
#else
std::function<void()> onceCallback(std::function<void()> fn, const char* msg)
{
    AX_UNUSED_PARAM(msg);
    auto called = std::make_shared<bool>(false);
    return [called, fn = std::move(fn)]() {
        if (*called)
            return;
        *called = true;
        fn();
    };
}
#endif
}  // namespace

namespace gameui
{

UIWidget::UIWidget() {}

UIWidget::~UIWidget() {}

void UIWidget::_create()
{
    m_state = UIState::Hidden;
    FGUIPackageManager::getInstance().load(getPackages());

    m_root = GComponent::create();
    m_root->setSize(GRoot::getInstance()->getWidth(), GRoot::getInstance()->getHeight());
    m_root->addRelation(GRoot::getInstance(), RelationType::Size);

    if (m_options.hasBackground)
    {
        m_background = GGraph::create();
        m_background->setSize(m_root->getWidth(), m_root->getHeight());
        m_background->addRelation(m_root, RelationType::Size);

        if (m_options.backgroundOpacity > 0)
        {
            m_background->drawRect(m_background->getWidth(), m_background->getHeight(), 0, ax::Color4F::BLACK,
                                   ax::Color4F(0, 0, 0, m_options.backgroundOpacity / 255.0f));
        }

        if (m_options.closeOnClickBg)
        {
            m_background->addClickListener([this](EventContext*) { close(); });
        }
        m_root->addChild(m_background);
    }

    GComponent* content = onCreateContent();
    if (content)
    {
        m_content = content;
        m_content->setDraggable(m_options.draggable);
        m_root->addChild(m_content);
    }

    onCreate();
}

void UIWidget::_destroy()
{
    // 从网络层解绑：取消未完成请求与推送监听
    // 这儿调用 detach 是因为 UIWidget 是智能指针管理的,UI
    // UIWidget被UIManager关闭时,需要立即取消网络请求,避免回调已经被定义为已关闭的无效UI
    detach();

    // 让子类有机会停止动画，避免动画结束后回调访问已经销毁的对象
    onStopAnimations();

    // 如果界面已经显示完毕了(即onShow被调用过),但是还未完全隐藏(即onHide尚未调用),
    // 为了让他们被成对调用,这儿手动调用onHide
    if (m_state == UIState::Visible || m_state == UIState::Hiding)
    {
        onHide();
    }

    onDestroy();
    if (m_root)
    {
        if (m_root->getParent())
            m_root->getParent()->removeChild(m_root);
        m_root = nullptr;
    }
    m_content      = nullptr;
    m_background   = nullptr;
    m_state        = UIState::None;
    m_ownerView    = nullptr;
    m_zorder       = 0;
    m_stackVisible = true;
    FGUIPackageManager::getInstance().unload(getPackages());
}

void UIWidget::close()
{
    if (m_state != UIState::Visible)
        return;
    _doHide();
}

void UIWidget::_doShow()
{
    m_state = UIState::Showing;
    m_root->setVisible(true);
    std::weak_ptr<UIWidget> weak = weak_from_this();
    doShowAnimation(onceCallback([weak]() {
        auto self = weak.lock();
        if (!self || self->m_state == UIState::None)
            return;
        self->m_state = UIState::Visible;
        self->onShow();
    }, "doShowAnimation: callback fired more than once"));
}

void UIWidget::_doHide()
{
    m_state                      = UIState::Hiding;
    std::weak_ptr<UIWidget> weak = weak_from_this();
    doHideAnimation(onceCallback([weak]() {
        auto self = weak.lock();
        if (!self || self->m_state == UIState::None)
            return;
        self->m_state = UIState::Hidden;
        self->m_root->setVisible(false);
        self->onHide();
        UIManager* mgr = self->m_manager;
        if (mgr)
            mgr->_onWidgetHidden(self.get());
    }, "doHideAnimation: callback fired more than once"));
}

bool UIWidget::isVisible() const
{
    return m_state == UIState::Visible || m_state == UIState::Showing;
}

void UIWidget::setBackgroundVisible(bool visible)
{
    if (m_background)
        m_background->setVisible(visible);
}

void UIWidget::addClickListener(UIEventDispatcher* dispatcher, const std::function<void(EventContext*)>& callback)
{
    if (dispatcher)
    {
        std::weak_ptr<UIWidget> weak = weak_from_this();
        dispatcher->addEventListener(UIEventType::Click, [weak, callback](EventContext* context) {
            auto self = weak.lock();
            if (!self || self->getUIManager() == nullptr)
                return;
            if (callback)
                callback(context);
        });
    }
}

GComponent* UIWidget::createCenteredComponent(const std::string& pkgName, const std::string& resName)
{
    GComponent* comp = UIPackage::createObject(pkgName, resName)->as<GComponent>();
    comp->setPivot(0.5f, 0.5f, true);
    comp->setPosition(m_root->getWidth() / 2, m_root->getHeight() / 2);
    comp->addRelation(m_root, RelationType::Center_Center);
    return comp;
}

void UIWidget::doShowAnimation(std::function<void()> done)
{
    if (done)
        done();
}

void UIWidget::doHideAnimation(std::function<void()> done)
{
    if (done)
        done();
}

}  // namespace gameui
